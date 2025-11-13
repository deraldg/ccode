#pragma once
#include <string>
#include <sstream>
#include <cctype>
#include "scan_state.hpp"
#include "textio.hpp"

// Strip "&& ..." outside of quotes
inline std::string scanhook_strip_inline_comment(std::string s) {
    bool in_s=false, in_d=false;
    for (size_t i=0; i+1<s.size(); ++i) {
        char c=s[i], n=s[i+1];
        if (!in_s && c=='"')  { in_d=!in_d; continue; }
        if (!in_d && c=='\'') { in_s=!in_s; continue; }
        if (!in_s && !in_d && c=='&' && n=='&') { s.erase(i); break; }
    }
    return textio::rtrim(s);
}

// Return true if the line was captured (so the shell should NOT dispatch it now)
inline bool scanhook_maybe_capture_line(const std::string& rawLine) {
    auto& st = scanblock::state();
    if (!st.active) return false;

    // Let SCAN and ENDSCAN themselves pass through to the registry
    std::string cmd;
    { std::istringstream iss(rawLine); iss >> cmd; }
    std::string up; up.reserve(cmd.size());
    for (unsigned char ch : cmd) up.push_back((char)std::toupper(ch));
    if (up == "SCAN" || up == "ENDSCAN") return false;

    // Otherwise buffer the comment-stripped line
    std::string line = scanhook_strip_inline_comment(rawLine);
    if (!line.empty()) st.lines.push_back(line);
    return true;
}
