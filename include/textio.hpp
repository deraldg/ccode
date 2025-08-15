#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace textio {

// --- trimming / case helpers -------------------------------------------------
inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char c){ return !std::isspace(c); }));
    return s;
}
inline std::string rtrim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
    return s;
}
inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

inline std::string up(std::string s) {
    for (auto& ch : s)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}
inline bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])))
         != static_cast<char>(std::tolower(static_cast<unsigned char>(b[i]))))
            return false;
    }
    return true;
}
inline bool ends_with_ci(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    size_t off = s.size() - suf.size();
    for (size_t i = 0; i < suf.size(); ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[off + i])));
        char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suf[i])));
        if (a != b) return false;
    }
    return true;
}

// --- quoting / tokenizing ----------------------------------------------------
// Remove surrounding single/double quotes if present.
// Supports doubled quotes inside quotes:  "he said ""hi""" -> he said "hi"
inline std::string unquote(std::string s) {
    s = trim(std::move(s));
    if (s.size() < 2) return s;

    char q = s.front();
    if ((q != '"' && q != '\'') || s.back() != q) return s;

    std::string out;
    out.reserve(s.size() - 2);
    for (size_t i = 1; i + 1 < s.size(); ++i) {
        char c = s[i];
        if (c == q && i + 1 < s.size() && s[i + 1] == q) {
            out.push_back(q); // doubled quote -> one
            ++i;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

// Quote-aware split on whitespace; respects '...' or "..." with doubled quotes.
inline std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quote = false;
    char q = 0;

    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (in_quote) {
            if (c == static_cast<unsigned char>(q)) {
                if (i + 1 < s.size() && s[i + 1] == q) { // doubled quote
                    cur.push_back(static_cast<char>(q));
                    ++i;
                } else {
                    in_quote = false;
                }
            } else {
                cur.push_back(static_cast<char>(c));
            }
        } else {
            if (c == '"' || c == '\'') {
                in_quote = true; q = static_cast<char>(c);
            } else if (std::isspace(c)) {
                if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            } else {
                cur.push_back(static_cast<char>(c));
            }
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // namespace textio
