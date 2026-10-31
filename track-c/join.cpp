#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
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

class Csv {
    FILE* file;
    int ch;
    void next() { ch = getc_unlocked(file); }
public:
    explicit Csv(const fs::path& path) : file(std::fopen(path.c_str(), "rb")) {
        if (!file) throw std::runtime_error("cannot open records.csv");
        next();
    }
    ~Csv() { std::fclose(file); }
    bool done() const { return ch == EOF; }
    void take(int expected) {
        if (ch != expected) throw std::runtime_error("unexpected CSV delimiter");
        next();
    }
    void bom() {
        if (ch == 0xef) { next(); take(0xbb); take(0xbf); }
    }
    void end_record() {
        if (ch == '\r') next();
        if (!done()) take('\n');
        if (std::ferror(file)) throw std::runtime_error("records.csv read failed");
    }
    i64 number() {
        if (ch < '0' || ch > '9') throw std::runtime_error("missing number");
        i64 value = 0;
        do { value = value * 10 + ch - '0'; next(); } while (ch >= '0' && ch <= '9');
        return value;
    }
    i64 cents() {
        bool negative = ch == '-';
        if (negative) next();
        i64 whole = number();
        take('.');
        i64 fraction = number();
        return (negative ? -1 : 1) * (100 * whole + fraction);
    }
    // The note's value is never materialized. Quoted newlines and escaped
    // quotes pass through exactly the same decoder as the distinct-name key.
    std::size_t text(std::string* value) {
        bool quoted = ch == '"';
        if (quoted) next();
        std::size_t size = 0;
        while (!done()) {
            if (quoted && ch == '"') {
                next();
                if (ch != '"') break;
            } else if (!quoted && ch == ',') {
                break;
            }
            if (value) value->push_back(static_cast<char>(ch));
            ++size;
            next();
        }
        take(',');
        return size;
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
    std::vector<unsigned> regions;
    while (std::getline(lookup, row)) {
        std::size_t id = 0;
        auto [pos, ec] = std::from_chars(row.data(), row.data() + row.size(), id);
        if (ec != std::errc{} || pos == row.data() + row.size() || *pos != ',') throw std::runtime_error("bad region id");
        auto country = countries.try_emplace(std::string(pos + 1, row.data() + row.size() - pos - 1), countries.size()).first->second;
        if (regions.size() <= id) regions.resize(id + 1);
        regions[id] = country;
    }
    std::vector<Totals> groups(countries.size());
    std::unordered_map<std::string, std::size_t> name_ids;
    Csv csv(data / "records.csv");
    csv.bom();
    for (auto c : std::string("id,name,note,region_id,amount")) csv.take(c);
    csv.end_record();
    std::string name;
    while (!csv.done()) {
        csv.bom();
        i64 id = csv.number(); csv.take(',');
        name.clear();
        auto name_bytes = csv.text(&name);
        auto note_bytes = csv.text(nullptr);
        auto region = static_cast<std::size_t>(csv.number()); csv.take(',');
        i64 amount = csv.cents();
        csv.end_record();
        auto name_id = name_ids.try_emplace(name, name_ids.size()).first->second;
        groups[region < regions.size() ? regions[region] : 0].add(id, amount, name_id, name_bytes, note_bytes);
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
