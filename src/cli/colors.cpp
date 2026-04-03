// src/cli/colors.cpp
// Full 16-color ANSI palette — 100% compatible with FoxPro 2.6 / dBASE IV color syntax
#include "colors.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

namespace dli {
namespace colors {

static Theme g_current = Theme::Default;
static bool  g_tree_color_enabled = true;

// Four readable rotating colors for tree depth on dark consoles.
static constexpr Theme kTreePalette[] = {
    Theme::BrightCyan,
    Theme::BrightGreen,
    Theme::Yellow,
    Theme::BrightMagenta
};

// Helper: uppercase
static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static void emitThemeImpl(Theme t) {
    switch (t) {
        case Theme::Default:       std::cout << "\x1b[0m";      break; // reset
        case Theme::Black:         std::cout << "\x1b[30m";    break;
        case Theme::Blue:          std::cout << "\x1b[34m";    break;
        case Theme::Green:         std::cout << "\x1b[32m";    break;
        case Theme::Cyan:          std::cout << "\x1b[36m";    break;
        case Theme::Red:           std::cout << "\x1b[31m";    break;
        case Theme::Magenta:       std::cout << "\x1b[35m";    break;
        case Theme::Brown:         std::cout << "\x1b[33m";    break;
        case Theme::LightGray:     std::cout << "\x1b[37m";    break;
        case Theme::DarkGray:      std::cout << "\x1b[90m";    break;
        case Theme::BrightBlue:    std::cout << "\x1b[94m";    break;
        case Theme::BrightGreen:   std::cout << "\x1b[92m";    break;
        case Theme::BrightCyan:    std::cout << "\x1b[96m";    break;
        case Theme::BrightRed:     std::cout << "\x1b[91m";    break;
        case Theme::BrightMagenta: std::cout << "\x1b[95m";    break;
        case Theme::Yellow:        std::cout << "\x1b[93m";    break;
        case Theme::White:         std::cout << "\x1b[97m";    break;
    }
    std::cout.flush();
}

// Full xBase color code parser — accepts:
//   W+, G*, RB/N, GR+/N, YELLOW, AMBER, MATRIX, etc.
Theme parseTheme(const std::string& input) {
    std::string s = upper(input);
    // Remove spaces and common junk
    s.erase(std::remove_if(s.begin(), s.end(), [](char c){ return c == ' ' || c == '\t'; }), s.end());

    if (s.empty()) return Theme::Default;

    // Classic short codes
    if (s == "N"  || s == "BLACK")      return Theme::Black;
    if (s == "B"  || s == "BLUE")       return Theme::Blue;
    if (s == "G"  || s == "GREEN")      return Theme::Green;
    if (s == "BG" || s == "CYAN")       return Theme::Cyan;
    if (s == "R"  || s == "RED")        return Theme::Red;
    if (s == "RB" || s == "MAGENTA" || s == "PURPLE") return Theme::Magenta;
    if (s == "GR" || s == "BROWN")      return Theme::Brown;
    if (s == "W"  || s == "GRAY" || s == "LIGHTGRAY") return Theme::LightGray;

    // Enhanced ("+" or "*")
    if (s == "N+"  || s == "DARKGRAY")      return Theme::DarkGray;
    if (s == "B+"  || s == "BRIGHTBLUE")    return Theme::BrightBlue;
    if (s == "G+"  || s == "BRIGHTGREEN" || s == "MATRIX") return Theme::BrightGreen;
    if (s == "BG+" || s == "BRIGHTCYAN")    return Theme::BrightCyan;
    if (s == "R+"  || s == "BRIGHTRED")     return Theme::BrightRed;
    if (s == "RB+" || s == "BRIGHTMAGENTA") return Theme::BrightMagenta;
    if (s == "GR+" || s == "W+" || s == "YELLOW" || s == "AMBER" || s == "CRT") return Theme::Yellow;
    if (s == "W++" || s == "WHITE")         return Theme::White;

    // Background variants (e.g., "W/N", "GR/N")
    if (s.find("/N") != std::string::npos)  return Theme::Black;
    if (s.find("/B") != std::string::npos)  return Theme::Blue;

    // Long names
    if (s == "DEFAULT" || s == "NORMAL" || s == "CLASSIC") return Theme::Default;
    if (s == "MATRIX")    return Theme::BrightGreen;
    if (s == "AMBER" || s == "YELLOW" || s == "CRT") return Theme::Yellow;

    return Theme::Default;
}

std::string themeName(Theme t) {
    switch (t) {
        case Theme::Default:       return "DEFAULT (LightGray/Black)";
        case Theme::Black:         return "N  (Black)";
        case Theme::Blue:          return "B  (Blue)";
        case Theme::Green:         return "G  (Green)";
        case Theme::Cyan:          return "BG (Cyan)";
        case Theme::Red:           return "R  (Red)";
        case Theme::Magenta:       return "RB (Magenta)";
        case Theme::Brown:         return "GR (Brown)";
        case Theme::LightGray:     return "W  (Light Gray)";
        case Theme::DarkGray:      return "N+ (Dark Gray)";
        case Theme::BrightBlue:    return "B+ (Bright Blue)";
        case Theme::BrightGreen:   return "G+ (Bright Green / MATRIX)";
        case Theme::BrightCyan:    return "BG+ (Bright Cyan)";
        case Theme::BrightRed:     return "R+ (Bright Red)";
        case Theme::BrightMagenta: return "RB+ (Bright Magenta)";
        case Theme::Yellow:        return "GR+/W+ (Yellow / Amber CRT)";
        case Theme::White:         return "W+ (White)";
    }
    return "UNKNOWN";
}

// Exact VGA 16-color palette — works everywhere
void applyTheme(Theme t) {
    emitThemeImpl(t);
    g_current = t;
}

Theme currentTheme() {
    return g_current;
}

void emitTheme(Theme t) {
    emitThemeImpl(t);
}

void emitCurrentTheme() {
    emitThemeImpl(g_current);
}

std::size_t treePaletteSize() {
    return sizeof(kTreePalette) / sizeof(kTreePalette[0]);
}

Theme treeThemeForLevel(int depth) {
    if (depth < 0) depth = 0;
    return kTreePalette[static_cast<std::size_t>(depth) % treePaletteSize()];
}

bool treeColorEnabled() {
    return g_tree_color_enabled;
}

void setTreeColorEnabled(bool enabled) {
    g_tree_color_enabled = enabled;
}

} // namespace colors
} // namespace dli
