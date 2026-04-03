#pragma once
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace dottalk::help {

inline std::string trim(std::string s) {
    auto notsp = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

inline std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

inline std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::string tok;
    std::istringstream iss(s);
    while (iss >> tok) out.push_back(tok);
    return out;
}

inline bool eqi(const std::string& a, const std::string& b) {
    return upper(a) == upper(b);
}

struct HelpOpts {
    bool onlyLocal = false;
    bool onlyFox   = false;
    bool onlyDot   = false;
    bool onlySql   = false;
    bool onlyPs    = false;
    bool onlyEd    = false;
    bool isBuild   = false;
    bool predOnly  = false;
    std::string term;
};

inline HelpOpts parse_opts(const std::string& raw) {
    HelpOpts o;
    auto toks = split_ws(trim(raw));
    if (toks.empty()) return o;

    if (eqi(toks[0], "BUILD")) {
        o.isBuild = true;
        return o;
    }

    size_t i = 0;
    while (i < toks.size() && !toks[i].empty() && toks[i][0] == '/') {
        if (eqi(toks[i], "/LOCAL")) {
            o.onlyLocal = true;
        } else if (eqi(toks[i], "/FOX")) {
            o.onlyFox = true;
        } else if (eqi(toks[i], "/DOT")) {
            o.onlyDot = true;
        } else if (eqi(toks[i], "/SQL")) {
            o.onlySql = true;
        } else if (eqi(toks[i], "/PS") ||
                   eqi(toks[i], "/PSHELL") ||
                   eqi(toks[i], "/POWERSHELL")) {
            o.onlyPs = true;
        } else if (eqi(toks[i], "/ED") ||
                   eqi(toks[i], "/EDU") ||
                   eqi(toks[i], "/EDUCATION")) {
            o.onlyEd = true;
        } else if (eqi(toks[i], "/PRED") ||
                   eqi(toks[i], "/PREDICATES")) {
            o.predOnly = true;
        }
        ++i;
    }

    for (; i < toks.size(); ++i) {
        if (!o.term.empty()) o.term.push_back(' ');
        o.term += toks[i];
    }

    if (!o.predOnly && eqi(o.term, "PREDICATES")) {
        o.predOnly = true;
        o.term.clear();
    }

    return o;
}

} // namespace dottalk::help