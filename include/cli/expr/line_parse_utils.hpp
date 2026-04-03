#pragma once
#include <string>
#include <string_view>
#include <cctype>

// True if s is a non-empty string of decimal digits.
inline bool is_uint(std::string_view s) {
    if (s.empty()) return false;
    for (unsigned char c : s) if (!std::isdigit(c)) return false;
    return true;
}

// Remove end-of-line comments outside quotes.
// Recognizes FoxPro-style '&&' and C++-style '//'.
// If a line starts with '*' or ';' (after optional spaces), it's a comment.
inline std::string strip_line_comments(std::string s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i < s.size() && (s[i] == '*' || s[i] == ';')) return std::string();

    bool in_s = false, in_d = false;
    for (size_t j = 0; j + 1 <= s.size(); ++j) {
        char c = s[j];
        if (c == '\'' && !in_d) { in_s = !in_s; continue; }
        if (c == '"'  && !in_s) { in_d = !in_d; continue; }
        if (!in_s && !in_d) {
            if (j + 1 < s.size() && s[j] == '&' && s[j+1] == '&') { s.resize(j); break; }
            if (j + 1 < s.size() && s[j] == '/' && s[j+1] == '/') { s.resize(j); break; }
        }
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

// Detect + - * / outside quotes (used to decide if an expression has arithmetic).
inline bool has_unquoted_arith(std::string_view s) {
    bool in_s = false, in_d = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\'' && !in_d) { in_s = !in_s; continue; }
        if (c == '"'  && !in_s) { in_d = !in_d; continue; }
        if (!in_s && !in_d) {
            if (c == '+' || c == '-' || c == '*' || c == '/') return true;
        }
    }
    return false;
}



