#include "parse.hpp"
#include "textio.hpp"
#include <cctype>
#include <algorithm>
#include <unordered_set>

using textio::trim;
using textio::up;

static bool is_int(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0]=='+'||s[0]=='-') ? 1 : 0;
    for (; i<s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}

// split on whitespace but keep quoted segments as single tokens; quotes kept
std::vector<std::string> cli_tokenize(const std::string& s) {
    std::vector<std::string> out; std::string cur; bool inq=false; char q=0;
    for (size_t i=0;i<s.size();++i) {
        char c = s[i];
        if (!inq && std::isspace((unsigned char)c)) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            continue;
        }
        if (c=='\'' || c=='"') {
            if (!inq) { inq=true; q=c; }
            else if (q==c) { inq=false; }
        }
        cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

ParseResult parse_scan_options(std::istringstream& S, const std::string& verb) {
    ParseResult pr; pr.opt.usageVerb = verb;

    std::string rest; std::getline(S, rest); rest = trim(rest);
    if (rest.empty()) { pr.ok = true; return pr; }

    auto toks = cli_tokenize(rest);
    const std::unordered_set<std::string> kw = {"ALL","DELETED","FOR","WHILE","NEXT","RECORD","REST"};

    auto next_join_until_kw = [&](size_t& i)->std::string {
        std::string acc;
        for (; i<toks.size(); ++i) {
            std::string upc = up(toks[i]);
            if (kw.count(upc)) break;
            if (!acc.empty()) acc.push_back(' ');
            acc += toks[i];
        }
        return trim(acc);
    };

    for (size_t i=0; i<toks.size();) {
        std::string t = toks[i], T = up(t);
        if (T=="ALL") { pr.opt.del_mode = ScanOptions::DeleteMode::IncludeDeleted; ++i; }
        else if (T=="DELETED") { pr.opt.del_mode = ScanOptions::DeleteMode::OnlyDeleted; ++i; }
        else if (T=="FOR") {
            ++i; auto expr = next_join_until_kw(i);
            if (expr.empty()) { pr.err="FOR requires an expression"; return pr; }
            pr.opt.for_expr = expr;
        }
        else if (T=="WHILE") {
            ++i; auto expr = next_join_until_kw(i);
            if (expr.empty()) { pr.err="WHILE requires an expression"; return pr; }
            pr.opt.while_expr = expr;
        }
        else if (T=="NEXT") {
            ++i; if (i>=toks.size() || !is_int(toks[i])) { pr.err="NEXT requires integer"; return pr; }
            pr.opt.range = ScanOptions::Range::NextN; pr.opt.n = std::stoi(toks[i++]);
        }
        else if (T=="RECORD") {
            ++i; if (i>=toks.size() || !is_int(toks[i])) { pr.err="RECORD requires integer"; return pr; }
            pr.opt.range = ScanOptions::Range::RecordN; pr.opt.n = std::stoi(toks[i++]);
        }
        else if (T=="REST") { pr.opt.range = ScanOptions::Range::Rest; ++i; }
        else { pr.err = "Unknown token: " + t; return pr; }
    }

    pr.ok = true;
    return pr;
}
