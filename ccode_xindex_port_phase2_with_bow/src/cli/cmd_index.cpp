#include "xbase.hpp"
#include "textio.hpp"
#include "xindex/index_manager.hpp"
#include "xindex/index_spec.hpp"
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

using xindex::IndexSpec;

static std::string up(std::string s) { for (auto& c: s) c = (char)std::toupper((unsigned char)c); return s; }
static std::string trim(std::string s) {
    auto issp=[](unsigned char c){return std::isspace(c)!=0;};
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static std::vector<std::string> split_csv(std::string s) {
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

void cmd_INDEX(xbase::DbArea& area, std::istringstream& iss) {
    if (!area.is_open()) { textio::print("No table open.\n"); return; }

    std::string tok;
    if (!(iss >> tok) || up(tok)!="ON") {
        textio::print("Syntax: INDEX ON <field[,field...]> [TAG <name>] [ASCEND|DESCEND] [UNIQUE]\n");
        return;
    }
    std::string rest;
    std::getline(iss, rest);
    std::string uprest = up(rest);

    size_t posTag = uprest.find(" TAG ");
    size_t posAsc = uprest.find(" ASCEND");
    size_t posDesc= uprest.find(" DESCEND");
    size_t posUni = uprest.find(" UNIQUE");

    size_t optStart = std::string::npos;
    for (size_t p : {posTag,posAsc,posDesc,posUni}) {
        if (p!=std::string::npos) optStart = (optStart==std::string::npos)? p : std::min(optStart,p);
    }

    std::string fieldsPart = (optStart==std::string::npos)? rest : rest.substr(0,optStart);
    auto fields = split_csv(fieldsPart);
    if (fields.empty()) { textio::print("INDEX: no fields provided.\n"); return; }

    IndexSpec spec;
    spec.fields = fields;
    spec.tag = fields.front(); // default
    spec.ascending = (posDesc==std::string::npos);
    spec.unique = (posUni!=std::string::npos);

    if (posTag!=std::string::npos) {
        size_t p = posTag + 5; // " TAG "
        while (p < rest.size() && std::isspace((unsigned char)rest[p])) ++p;
        size_t q = p;
        while (q < rest.size() && !std::isspace((unsigned char)rest[q])) ++q;
        if (q>p) spec.tag = rest.substr(p, q-p);
    }

    auto* idx = area.idx();
    if (!idx) { textio::print("INDEX: internal error (missing index manager).\n"); return; }
    auto& tag = idx->ensure_tag(spec);
    (void)tag;
    idx->set_active(spec.tag);
    idx->set_direction(spec.ascending);
    idx->save(area.current_dbf_path());

    textio::printf("Built index TAG %s on %zu field(s). Order: %s.\n",
        spec.tag.c_str(), spec.fields.size(), spec.ascending? "ASC":"DESC");
}
