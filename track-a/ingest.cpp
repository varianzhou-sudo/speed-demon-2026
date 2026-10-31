#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct Bucket {
    std::vector<std::uint32_t> latencies;
    std::uint64_t errors = 0;
};

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    std::ifstream in(argv[1]);
    std::ofstream out(argv[2]);
    if (!in || !out) return 1;

    using EndpointGroups = std::map<std::string, Bucket, std::less<>>;
    std::map<std::string, EndpointGroups, std::less<>> hours;
    std::string line;
    while (std::getline(in, line)) {
        std::string_view record(line);
        auto hour = record.substr(0, 13);
        auto endpoint_start = record.find(' ', 28) + 1;
        auto endpoint_end = record.find(' ', endpoint_start);
        auto endpoint = record.substr(endpoint_start, endpoint_end - endpoint_start);
        auto latency_start = endpoint_end + 5;
        std::uint32_t latency;
        auto parse = std::from_chars(record.data() + latency_start, record.data() + record.size(), latency);
        if (parse.ec != std::errc{} || parse.ptr != record.data() + record.size()) return 2;
        auto h = hours.find(hour);
        if (h == hours.end()) h = hours.try_emplace(std::string(hour)).first;
        auto& groups = h->second;
        auto g = groups.find(endpoint);
        if (g == groups.end()) g = groups.try_emplace(std::string(endpoint)).first;
        g->second.latencies.push_back(latency);
        g->second.errors += record[endpoint_end + 1] == '5';
    }
    for (auto& [hour, groups] : hours) {
        for (auto& [endpoint, group] : groups) {
            auto& values = group.latencies;
            std::sort(values.begin(), values.end());
            const auto n = values.size();
            const auto percentile = [&](int p){ return values[(p * n - 1) / 100]; };
            out << hour << ":00:00Z " << endpoint << ' ' << n << ' ' << percentile(50)
                << ' ' << percentile(95) << ' ' << percentile(99) << ' ' << group.errors
                << ' ' << group.errors * 1000000 / n << '\n';
        }
    }
    return in.bad() || !out;
}
