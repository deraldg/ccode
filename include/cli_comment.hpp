// include/cli_comment.hpp
#pragma once
#include <string>
#include <cctype>

namespace cliutil {

// Strip trailing inline comments outside quotes: supports FoxPro "&&" and C++ "//".
inline std::string strip_inline_comments(std::string s) {
    bool inq = false; char q = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        // toggle quoting state
        if (c == '"' || c == '\'') {
            if (!inq) { inq = true; q = c; }
            else if (c == q) { inq = false; }
            continue;
        }
        if (inq) continue;

        // FoxPro-style: &&
        if (c == '&' && i + 1 < s.size() && s[i + 1] == '&') {
            s.resize(i);
            break;
        }
        // C++-style: //
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            s.resize(i);
            break;
        }
    }

    auto issp = [](unsigned char ch){ return ch==' '||ch=='\t'||ch=='\r'||ch=='\n'; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}

} // namespace cliutil



