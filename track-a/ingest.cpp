#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <map>
#include <numeric>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using View = std::string_view;
struct Bucket {
    std::vector<uint32_t> latencies;
    uint64_t errors = 0;
};
class Paths {
    struct Slot { View key; size_t id; uint64_t hash; };
    std::vector<Slot> slots_ = std::vector<Slot>(64);
    std::deque<std::string> names_;
    static uint64_t hash(View key) {
        // Sample the beginning, middle and end. Equality always checks the entire key.
        uint64_t bits = key.size();
        if (key.size() >= 4) {
            for (size_t off : {size_t(0),(key.size()-4)/2,key.size()-4}) {
                uint32_t part;
                std::memcpy(&part, key.data()+off, sizeof(part));
                bits = (bits ^ part) * 0x9e3779b97f4a7c15ULL;
            }
        } else {
            for (unsigned char ch : key) bits = bits*131 + ch;
        }
        return bits ^ (bits >> 33);
    }
    Slot& lookup(View name, uint64_t h) {
        const size_t mask = slots_.size()-1;
        size_t pos = h & mask;
        while (!slots_[pos].key.empty() && (slots_[pos].hash != h || slots_[pos].key != name))
            pos = (pos+1) & mask;
        return slots_[pos];
    }
public:
    size_t intern(View name) {
        const uint64_t h = hash(name);
        auto& slot = lookup(name,h);
        if (!slot.key.empty()) return slot.id;
        size_t id = names_.size();
        names_.emplace_back(name);
        slot = {names_.back(),id,h};
        if (names_.size()*2 > slots_.size()) {
            auto old = std::move(slots_);
            slots_.resize(2*old.size());
            for (auto& entry : old)
                if (!entry.key.empty()) lookup(entry.key,entry.hash) = entry;
        }
        return id;
    }
    const auto& names() const { return names_; }
};
struct Shard {
    Paths endpoints;
    std::map<View, std::vector<Bucket>> by_hour;
};

// Each shard owns complete records. All string views borrow the read-only file mapping.
void scan(Shard& shard, const char* bytes, size_t begin, size_t end, size_t size) {
    const char* position = bytes + begin;
    const char* limit = bytes + end;
    const char* eof = bytes + size;
    if (begin && position[-1] != '\n')
        position = static_cast<const char*>(std::memchr(position, '\n', eof - position)) + 1;
    View last_hour;
    std::vector<Bucket>* hour_groups = nullptr;
    while (position < limit) {
        View hour(position, 13);
        if (hour != last_hour) {
            hour_groups = &shard.by_hour[hour];
            last_hour = hour;
        }
        const char* path = position + 28;
        while (*path++ != ' ') {}
        const char* separator = static_cast<const char*>(std::memchr(path, ' ', eof - path));
        View endpoint(path, static_cast<size_t>(separator - path));
        const size_t id = shard.endpoints.intern(endpoint);
        if (hour_groups->size() <= id) hour_groups->resize(id + 1);
        Bucket& group = (*hour_groups)[id];
        group.errors += separator[1] == '5';
        const char* digit = separator + 5;
        uint32_t latency = 0;
        while (*digit != '\n') latency = latency * 10 + unsigned(*digit++ - '0');
        group.latencies.push_back(latency);
        position = digit + 1;
    }
}

void append_number(std::string& line, uint64_t value) {
    char digits[32];
    auto end = std::to_chars(digits, digits + sizeof(digits), value).ptr;
    line += ' ';
    line.append(digits, end);
}
using Hour = std::vector<std::vector<Bucket>*>;
std::string render_hour(View hour, const Hour& groups, const std::vector<View>& names,
                        const std::vector<std::vector<int>>& ids) {
    std::string out;
    out.reserve(names.size() * 72);
    std::vector<uint32_t> merged;
    for (size_t e = 0; e < names.size(); ++e) {
        Bucket* first = nullptr;
        uint64_t errors = 0;
        merged.clear();
        for (size_t s = 0; s < groups.size(); ++s) {
            int id = ids[s][e];
            if (!groups[s] || id < 0 || unsigned(id) >= groups[s]->size()) continue;
            auto& part = (*groups[s])[id];
            if (part.latencies.empty()) continue;
            errors += part.errors;
            if (!first) {
                first = &part;
                continue;
            }
            if (merged.empty())
                merged.insert(merged.end(), first->latencies.begin(), first->latencies.end());
            merged.insert(merged.end(), part.latencies.begin(), part.latencies.end());
        }
        if (!first) continue;
        auto& values = merged.empty() ? first->latencies : merged;
        const size_t n = values.size();
        if (n < 128) std::sort(values.begin(), values.end());
        else {
            auto begin = values.begin();
            for (size_t p : {50, 95, 99}) {
                auto rank = values.begin() + (p*n-1)/100;
                std::nth_element(begin, rank, values.end());
                begin = rank+1;
            }
        }
        out.append(hour);
        out += ":00:00Z ";
        out += names[e];
        append_number(out, n);
        for (size_t p : {50, 95, 99}) append_number(out, values[(p*n-1)/100]);
        append_number(out, errors);
        append_number(out, errors*1000000/n);
        out += '\n';
    }
    return out;
}

template<typename F> void parallel(size_t n, F run) {
    std::vector<std::thread> threads;
    for (size_t i = 1; i < n; ++i) threads.emplace_back(run, i);
    run(0);
    for (auto& t : threads) t.join();
}

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    int fd = open(argv[1], O_RDONLY);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) return 1;
    FILE* output = std::fopen(argv[2], "wb");
    if (!output) return 1;
    size_t size = st.st_size;
    if (!size) return std::fclose(output) != 0;
    const char* data = static_cast<const char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (data == MAP_FAILED || data[size-1] != '\n') return 1;
    const size_t count = std::min(std::clamp(std::thread::hardware_concurrency(), 1u, 12u),
                                  static_cast<unsigned>(1 + size / (8 * 1024 * 1024)));
    std::vector<Shard> shards(count);
    parallel(count, [&](size_t i) { scan(shards[i], data, size*i/count, size*(i+1)/count, size); });

    std::map<View, Hour> hour_parts;
    std::vector<View> names;
    for (size_t i = 0; i < count; ++i) {
        for (const auto& name : shards[i].endpoints.names()) names.push_back(name);
        for (auto& [hour, groups] : shards[i].by_hour) {
            auto& parts = hour_parts.try_emplace(hour, count, nullptr).first->second;
            parts[i] = &groups;
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    std::vector<std::vector<int>> ids(count, std::vector<int>(names.size(), -1));
    for (size_t i = 0; i < count; ++i) {
        size_t id = 0;
        for (const auto& name : shards[i].endpoints.names()) {
            auto e = std::lower_bound(names.begin(), names.end(), name) - names.begin();
            ids[i][e] = id++;
        }
    }
    std::vector<std::map<View, Hour>::iterator> hours;
    for (auto it = hour_parts.begin(); it != hour_parts.end(); ++it) hours.push_back(it);
    std::vector<std::string> rendered(hours.size());
    std::atomic<size_t> next{0};
    parallel(count, [&](size_t) {
        for (auto i = next.fetch_add(1); i < hours.size(); i = next.fetch_add(1))
            rendered[i] = render_hour(hours[i]->first, hours[i]->second, names, ids);
    });
    for (auto& fragment : rendered) std::fwrite(fragment.data(), 1, fragment.size(), output);
    munmap(const_cast<char*>(data), size);
    return std::ferror(output) || std::fclose(output) != 0;
}
