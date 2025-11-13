#include "cli/preparse.hpp"
#include <string>
#include <vector>
#include <cctype>

static inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static std::string rtrim_copy(std::string s) {
    while (!s.empty() && is_space(s.back())) s.pop_back();
    return s;
}

static std::string ltrim_copy(const std::string& s) {
    size_t i = 0; while (i < s.size() && is_space(s[i])) ++i;
    return s.substr(i);
}

static std::string strip_inline_comments(const std::string& s) {
    bool in_s = false, in_d = false;
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        char c = s[i], n = s[i+1];
        if (c == '\"'  && !in_s) in_d = !in_d;
        if (c == '\'' && !in_d) in_s = !in_s;
        if (!in_s && !in_d && c == '&' && n == '&') {
            return rtrim_copy(s.substr(0, i));
        }
    }
    return rtrim_copy(s);
}

std::vector<std::string> cli_preparse(const std::string& raw) {
    std::vector<std::string> lines;
    {
        size_t start = 0;
        while (start < raw.size()) {
            size_t nl = raw.find('\n', start);
            if (nl == std::string::npos) {
                lines.push_back(raw.substr(start));
                break;
            }
            lines.push_back(raw.substr(start, nl - start));
            start = nl + 1;
        }
        if (lines.empty()) lines.push_back(std::string());
    }

    std::vector<std::string> logical;
    std::string current;

    auto flush = [&](){
        std::string tmp = ltrim_copy(rtrim_copy(current));
        if (!tmp.empty()) {
            std::string lt = ltrim_copy(tmp);
            if (!lt.empty() && lt[0] == '*') {
                // discard whole-line comment
            } else {
                logical.push_back(tmp);
            }
        }
        current.clear();
    };

    for (std::string ln : lines) {
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        std::string lt = ltrim_copy(ln);
        if (!lt.empty() && lt[0] == '*') {
            continue;
        }
        ln = strip_inline_comments(ln);

        bool in_s = false, in_d = false;
        for (char c : ln) {
            if (c == '\"'  && !in_s) in_d = !in_d;
            if (c == '\'' && !in_d) in_s = !in_s;
        }
        bool cont = false;
        if (!ln.empty() && !in_s && !in_d) {
            std::string rt = rtrim_copy(ln);
            if (!rt.empty() && rt.back() == ';') {
                cont = true;
                rt.pop_back();
                ln = rt;
            }
        }

        if (!current.empty() && !ln.empty()) current.push_back(' ');
        current += ln;

        if (!cont) flush();
    }
    if (!current.empty()) flush();
    return logical;
}
