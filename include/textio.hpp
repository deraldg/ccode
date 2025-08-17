#pragma once
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace textio {

// ---------------- basic trimming & case helpers ----------------

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

// ---------------- quoting / tokenizing helpers ----------------

/* Unescape a few common sequences: \n \t \r \\ \" \'
   (Use a block comment to avoid line-continuation pitfalls in MSVC.) */
inline std::string unescape_basic(std::string s) {
    std::string out;
    out.reserve(s.size());
    bool esc = false;
    for (char c : s) {
        if (!esc) {
            if (c == '\\') { esc = true; continue; }
            out.push_back(c);
        } else {
            switch (c) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case '\'': out.push_back('\''); break;
                default: out.push_back(c); break; // unknown escape: keep literal
            }
            esc = false;
        }
    }
    if (esc) out.push_back('\\'); // trailing backslash literal
    return out;
}

inline std::string unquote(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2) {
        char q0 = t.front();
        char q1 = t.back();
        if ((q0 == '"' && q1 == '"') || (q0 == '\'' && q1 == '\'')) {
            std::string inner = t.substr(1, t.size() - 2);
            return unescape_basic(inner);
        }
    }
    return t;
}

// Tokenize respecting quotes ('...'/ "....") and backslash escapes inside quotes.
// Whitespace separates tokens only when not inside quotes.
inline std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> toks;
    std::string cur;
    cur.reserve(line.size());

    bool in_quote = false;
    char quote_ch = '\0';
    bool esc = false;

    auto flush = [&]() {
        if (!cur.empty()) {
            toks.emplace_back(std::move(cur));
            cur.clear();
        }
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (in_quote) {
            if (esc) {
                switch (c) {
                    case 'n': cur.push_back('\n'); break;
                    case 't': cur.push_back('\t'); break;
                    case 'r': cur.push_back('\r'); break;
                    case '\\': cur.push_back('\\'); break;
                    case '"': cur.push_back('"'); break;
                    case '\'': cur.push_back('\''); break;
                    default: cur.push_back(c); break;
                }
                esc = false;
                continue;
            }
            if (c == '\\') { esc = true; continue; }
            if (c == quote_ch) { in_quote = false; quote_ch = '\0'; continue; }
            cur.push_back(c);
            continue;
        }

        if (c == '"' || c == '\'') {
            in_quote = true;
            quote_ch = c;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            flush();
            continue;
        }
        cur.push_back(c);
    }
    flush();
    return toks;
}

// Convenience overload: tokenize the remainder of an istringstream's current line.
inline std::vector<std::string> tokenize(std::istringstream& iss) {
    std::string rest;
    // Read the remaining buffer (works even if previous >> consumed some tokens)
    if (iss.rdbuf()->in_avail() > 0) {
        std::getline(iss, rest);
    } else {
        // Fallback: try from current position
        auto pos = iss.tellg();
        if (pos != std::istringstream::pos_type(-1)) {
            std::string s = iss.str();
            if (static_cast<size_t>(pos) < s.size())
                rest = s.substr(static_cast<size_t>(pos));
        }
    }
    return tokenize(rest);
}

} // namespace textio
