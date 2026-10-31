#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using Id = uint32_t;
using Ids = std::vector<Id>;

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};

struct Query {
    bool phrase;
    Ids terms;
    std::vector<char> ops;
};

class Index {
    std::unordered_map<std::string, Id, StringHash, std::equal_to<>> dictionary;
    std::vector<Ids> postings{1};
    Ids tokens;
    std::vector<size_t> offsets{0};

    Id intern(const std::string& word) {
        auto [it, inserted] = dictionary.try_emplace(word, postings.size());
        if (inserted) postings.emplace_back();
        return it->second;
    }

public:
    std::vector<Query> read_queries(std::istream& file) {
        std::vector<Query> result;
        std::string line;
        while (std::getline(file, line)) {
            const bool phrase = line.starts_with("PHRASE ");
            std::istringstream fields(phrase ? line.substr(8, line.size()-9) : line.substr(5));
            Query q{phrase, {}, {}};
            std::string word;
            while (fields >> word) {
                q.terms.push_back(intern(word));
                if (!phrase && fields >> word) q.ops.push_back(word.at(0));
            }
            result.push_back(std::move(q));
        }
        return result;
    }

    void read_documents(std::istream& file) {
        std::array<unsigned char, 256> lower{};
        for (int c='a'; c<='z'; ++c) lower[c]=c;
        for (int c='A'; c<='Z'; ++c) lower[c]=c-'A'+'a';
        for (int c='0'; c<='9'; ++c) lower[c]=c;
        std::string line;
        while (std::getline(file, line)) {
            const size_t tab = line.find('\t');
            if (tab == std::string::npos) throw std::runtime_error("Missing document delimiter");
            Id doc_id{};
            auto [end, ec] = std::from_chars(line.data(), line.data()+tab, doc_id);
            if (ec != std::errc{} || end != line.data()+tab || doc_id != offsets.size()-1)
                throw std::runtime_error("Unexpected document id");
            for (size_t pos=tab+1; pos<line.size();) {
                if (!lower[static_cast<unsigned char>(line[pos])]) { ++pos; continue; }
                const size_t start = pos;
                while (pos<line.size() && lower[static_cast<unsigned char>(line[pos])]) {
                    line[pos]=static_cast<char>(lower[static_cast<unsigned char>(line[pos])]);
                    ++pos;
                }
                const auto it = dictionary.find(std::string_view(line).substr(start,pos-start));
                const Id term = it == dictionary.end() ? 0 : it->second;
                // Zero records a token outside the query vocabulary, preserving adjacency.
                tokens.push_back(term);
                if (term) {
                    auto& docs = postings[term];
                    if (docs.empty() || docs.back()!=doc_id) docs.push_back(doc_id);
                }
            }
            offsets.push_back(tokens.size());
        }
    }

    Ids answer(const Query& q) const {
        if (q.phrase) {
            const Id anchor = *std::min_element(q.terms.begin(),q.terms.end(),
                [&](Id a,Id b){return postings[a].size()<postings[b].size();});
            Ids hits;
            for (Id doc : postings[anchor]) {
                auto start = tokens.begin()+offsets[doc], stop = tokens.begin()+offsets[doc+1];
                if (std::search(start,stop,q.terms.begin(),q.terms.end())!=stop) hits.push_back(doc);
            }
            return hits;
        }
        Ids active = postings[q.terms.front()], next;
        for (size_t i=0; i<q.ops.size(); ++i) {
            next.clear();
            const auto& right = postings[q.terms[i+1]];
            auto out = std::back_inserter(next);
            switch (q.ops[i]) {
                case 'A': std::set_intersection(active.begin(),active.end(),right.begin(),right.end(),out); break;
                case 'O': std::set_union(active.begin(),active.end(),right.begin(),right.end(),out); break;
                case 'N': std::set_difference(active.begin(),active.end(),right.begin(),right.end(),out); break;
                default: throw std::runtime_error("Unknown boolean operator");
            }
            active.swap(next);
        }
        return active;
    }
};

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    const fs::path data = argc>1 ? argv[1] : "/data";
    const fs::path out = argc>2 ? argv[2] : "/out";
    std::ifstream queries_file(data/"queries.txt"), docs_file(data/"docs.txt");
    if (!queries_file || !docs_file) { std::cerr<<"Missing inputs\n"; return 1; }
    Index index;
    const auto queries = index.read_queries(queries_file);
    index.read_documents(docs_file);
    fs::create_directories(out);
    std::ofstream results(out/"results.txt");
    results.exceptions(std::ios::failbit | std::ios::badbit);
    for (const auto& q : queries) {
        const Ids hits = index.answer(q);
        uint64_t sum = 0;
        for (Id id : hits) sum+=id;
        results<<hits.size()<<' '<<sum;
        for (size_t i=0; i<std::min<size_t>(10,hits.size()); ++i) results<<' '<<hits[i];
        results<<'\n';
    }
    results.close();
}
