// src/cli/colors.cpp
#include "colors.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>

namespace dli {
namespace colors {

static Theme g_current = Theme::Default;

static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

Theme parseTheme(const std::string& s) {
    const std::string u = upper(s);
    if (u == "DEFAULT" || u == "DEF" || u == "NORMAL")  return Theme::Default;
    if (u == "GREEN"   || u == "GRN")                   return Theme::Green;
    if (u == "AMBER"   || u == "YELLOW" || u == "YLW")  return Theme::Amber;
    return Theme::Default;
}

std::string themeName(Theme t) {
    switch (t) {
        case Theme::Default: return "DEFAULT";
        case Theme::Green:   return "GREEN";
        case Theme::Amber:   return "AMBER";
    }
    return "DEFAULT";
}

void applyTheme(Theme t) {
    // Basic ANSI palette (VT seq) — harmless if the console ignores it.
    switch (t) {
        case Theme::Default: std::cout << "\x1b[0m";    break; // reset
        case Theme::Green:   std::cout << "\x1b[0;92m"; break; // bright green
        case Theme::Amber:   std::cout << "\x1b[0;93m"; break; // bright yellow/amber
    }
    std::cout.flush();
    g_current = t;
}

Theme currentTheme() {
    return g_current;
}

} // namespace colors
} // namespace dli
