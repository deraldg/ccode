#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace foxtalk {

inline short S(int v)
{
    return static_cast<short>(v);
}

inline std::string trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };

    s.erase(
        s.begin(),
        std::find_if(s.begin(), s.end(),
            [&](char c) { return notSpace(static_cast<unsigned char>(c)); })
    );

    s.erase(
        std::find_if(s.rbegin(), s.rend(),
            [&](char c) { return notSpace(static_cast<unsigned char>(c)); }).base(),
        s.end()
    );

    return s;
}

inline std::string upcopy(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline int clampInt(int v, int lo, int hi)
{
    return (v < lo) ? lo : (v > hi ? hi : v);
}

} // namespace foxtalk