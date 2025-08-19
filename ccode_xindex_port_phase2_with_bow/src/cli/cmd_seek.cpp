#include "xbase.hpp"
#include "textio.hpp"
#include "xindex/index_manager.hpp"
#include "xindex/index_spec.hpp"
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>
#include <cctype>

static std::string trim(std::string s) {
    auto issp=[](unsigned char c){return std::isspace(c)!=0;};
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static std::vector<std::string> split_commas(std::string s) {
    std::vector<std::string> out;
    size_t start=0;
    while (start < s.size()) {
        size_t comma = s.find(',', start);
        std::string tok = (comma==std::string::npos)? s.substr(start) : s.substr(start, comma-start);
        out.push_back(trim(tok));
        if (comma==std::string::npos) break;
        start = comma+1;
    }
    out.erase(std::remove_if(out.begin(), out.end(), [](auto& t){return t.empty();}), out.end());
    return out;
}

void cmd_SEEK(xbase::DbArea& area, std::istringstream& iss) {
    if (!area.is_open()) { textio::print("No table open.\n"); return; }
    auto* mgr = area.idx();
    if (!mgr || !mgr->has_active()) { textio::print("No active index.\n"); return; }
    auto* tag = mgr->active();
    if (!tag) { textio::print("No active index.\n"); return; }

    std::string rest;
    std::getline(iss, rest);
    auto toks = split_commas(rest);
    auto key = mgr->make_key_from_tokens(tag->spec(), toks);
    auto recnoOpt = tag->seek_first_ge(key);
    if (!recnoOpt) { textio::print("Not found.\n"); return; }
    area.goto_rec(*recnoOpt);
    textio::printf("Found at %d\n", *recnoOpt);
}
