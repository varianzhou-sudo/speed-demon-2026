#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using i64 = std::int64_t;

struct Text {
    std::string_view encoded;
    std::size_t size;
};

// Outer quotes are omitted, but doubled quotes stay in the view. This encoding
// is canonical for a decoded name (an unquoted CSV field cannot contain quotes).
bool read_text(const char*& p, const char* end, Text& field) {
    bool quoted = *p == '"';
    if (quoted) ++p;
    const char* begin = p;
    std::size_t escaped = 0;
    for (;;) {
        const char* stop = static_cast<const char*>(std::memchr(p, quoted ? '"' : ',', end - p));
        if (!stop) return false;
        p = stop + 1;
        if (quoted && *p == '"') {
            ++escaped;
            ++p;
            continue;
        }
        field.encoded = {begin, static_cast<std::size_t>(stop - begin)};
        field.size = field.encoded.size() - escaped;
        return !quoted || *p++ == ',';
    }
}

bool number(const char*& p, const char* end, char delimiter, i64& n) {
    const auto result = std::from_chars(p, end, n);
    p = result.ptr;
    if (result.ec != std::errc{} || *p != delimiter) return false;
    ++p;
    return true;
}

struct Row {
    i64 id, region, amount;
    Text name, note;
};

bool read_row(const char*& p, const char* end, bool final, Row& row) {
    if (std::memcmp(p, "\xef\xbb\xbf", 3) == 0) p += 3;
    if (!number(p, end, ',', row.id) || !read_text(p, end, row.name) ||
        !read_text(p, end, row.note) || !number(p, end, ',', row.region)) return false;
    bool negative = *p == '-';
    if (negative) ++p;
    i64 whole;
    if (!number(p, end, '.', whole) || end - p < 2 || p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9')
        return false;
    row.amount = (negative ? -1 : 1) * (100 * whole + (p[0] - '0') * 10 + (p[1] - '0'));
    p += 2;
    if (*p == '\r') ++p;
    if (p == end) return final;
    return *p++ == '\n';
}

// Slots point into a compact byte pool. Hash collisions are resolved by
// comparing complete names, including trailing spaces and escaped quotes.
class Names {
    struct Slot { std::uint32_t id = 0, hash = 0; };
    struct Entry { std::uint32_t offset, length; };
    std::vector<Slot> slots{1024};
    std::vector<Entry> entries{{0, 0}};
    std::string bytes;
    std::size_t probe(std::uint32_t hash, std::string_view name) const {
        std::size_t i = hash & (slots.size() - 1);
        while (slots[i].id) {
            auto ref = entries[slots[i].id];
            if (slots[i].hash == hash && name.size() == ref.length &&
                !std::memcmp(bytes.data() + ref.offset, name.data(), ref.length)) break;
            i = (i + 1) & (slots.size() - 1);
        }
        return i;
    }
    void grow() {
        auto previous = std::move(slots);
        slots.resize(previous.size() * 2);
        for (auto slot : previous) if (slot.id) {
            auto i = slot.hash & (slots.size() - 1);
            while (slots[i].id) i = (i + 1) & (slots.size() - 1);
            slots[i] = slot;
        }
    }
public:
    std::size_t intern(std::string_view name) {
        auto hash = static_cast<std::uint32_t>(std::hash<std::string_view>{}(name));
        auto i = probe(hash, name);
        if (slots[i].id) return slots[i].id;
        if (entries.size() * 10 > slots.size() * 7) { grow(); i = probe(hash, name); }
        auto id = static_cast<std::uint32_t>(entries.size());
        entries.push_back({static_cast<std::uint32_t>(bytes.size()), static_cast<std::uint32_t>(name.size())});
        bytes.append(name);
        slots[i] = {id, hash};
        return id;
    }
};

struct Totals {
    i64 count = 0, ids = 0, sum = 0;
    i64 low = std::numeric_limits<i64>::max(), high = std::numeric_limits<i64>::min();
    std::size_t n_names = 0, name_bytes = 0, note_bytes = 0;
    std::vector<std::uint64_t> seen;
    void add(i64 id, i64 amount, std::size_t name_id, std::size_t n, std::size_t m) {
        ++count; ids += id; sum += amount;
        low = std::min(amount, low); high = std::max(amount, high);
        name_bytes += n; note_bytes += m;
        auto word = name_id / 64;
        auto bit = std::uint64_t{1} << (name_id % 64);
        if (seen.size() <= word) seen.resize(word + 1);
        if (!(seen[word] & bit)) { seen[word] |= bit; ++n_names; }
    }
};

std::string money(i64 value) {
    auto absolute = value < 0 ? -value : value;
    auto cents = absolute % 100;
    return (value < 0 ? "-" : "") + std::to_string(absolute / 100) + (cents < 10 ? ".0" : ".") + std::to_string(cents);
}

void join(const fs::path& data, const fs::path& out) {
    std::ifstream lookup(data / "regions.csv");
    std::string row;
    if (!std::getline(lookup, row) || row != "region_id,country") throw std::runtime_error("invalid regions header");
    std::map<std::string, std::size_t> countries{{"_UNMATCHED", 0}};
    std::vector<std::uint16_t> regions;
    while (std::getline(lookup, row)) {
        std::size_t id = 0;
        auto [pos, ec] = std::from_chars(row.data(), row.data() + row.size(), id);
        if (ec != std::errc{} || pos == row.data() + row.size() || *pos != ',') throw std::runtime_error("bad region id");
        auto country = countries.try_emplace(std::string(pos + 1, row.data() + row.size() - pos - 1), countries.size()).first->second;
        if (regions.size() <= id) regions.resize(id + 1);
        regions[id] = country;
    }
    std::vector<Totals> groups(countries.size());
    Names names;
    std::ifstream input(data / "records.csv", std::ios::binary);
    if (!std::getline(input, row)) throw std::runtime_error("missing records header");
    if (row.compare(0, 3, "\xef\xbb\xbf") == 0) row.erase(0, 3);
    if (!row.empty() && row.back() == '\r') row.pop_back();
    if (row != "id,name,note,region_id,amount") throw std::runtime_error("invalid records header");

    // A sentinel and two padding bytes make the EOF, BOM and quote lookahead
    // safe. Carry an incomplete row to the next block, growing for long fields.
    std::vector<char> buffer(256 * 1024 + 3);
    std::size_t tail = 0;
    for (;;) {
        input.read(buffer.data() + tail, buffer.size() - 3 - tail);
        if (input.bad()) throw std::runtime_error("records.csv read failed");
        const bool final = input.eof();
        const std::size_t size = tail + input.gcount();
        std::memset(buffer.data() + size, 0, 3);
        const char* p = buffer.data();
        const char* end = p + size;
        Row record;
        while (p != end) {
            const char* begin = p;
            if (!read_row(p, end, final, record)) { p = begin; break; }
            auto name_id = names.intern(record.name.encoded);
            auto region = static_cast<std::size_t>(record.region);
            groups[region < regions.size() ? regions[region] : 0].add(record.id, record.amount, name_id, record.name.size, record.note.size);
        }
        tail = end - p;
        if (final) {
            if (tail) throw std::runtime_error("invalid CSV row");
            break;
        }
        std::memmove(buffer.data(), p, tail);
        if (tail == buffer.size() - 3) buffer.resize(2 * tail + 3);
    }
    fs::create_directories(out);
    std::ofstream output(out / "aggregates.txt", std::ios::binary);
    for (const auto& [code, index] : countries) {
        const auto& g = groups[index];
        if (g.count) output << code << ' ' << g.count << ' ' << g.ids << ' ' << money(g.sum) << ' ' << money(g.low)
                            << ' ' << money(g.high) << ' ' << g.n_names << ' ' << g.name_bytes << ' ' << g.note_bytes << '\n';
    }
    output.close();
    if (!output) throw std::runtime_error("aggregates.txt write failed");
}

int main(int argc, char** argv) {
    try {
        if (argc != 1 && argc != 3) throw std::runtime_error("usage: join [DATA_DIR OUT_DIR]");
        join(argc == 3 ? argv[1] : "/data", argc == 3 ? argv[2] : "/out");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
