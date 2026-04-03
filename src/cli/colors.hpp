// src/cli/colors.hpp
#pragma once
#include <cstddef>
#include <string>

namespace dli {
namespace colors {

enum class Theme {
    Default,      // Classic light gray on black (FoxPro default)
    Black,
    Blue,
    Green,
    Cyan,
    Red,
    Magenta,
    Brown,
    LightGray,
    DarkGray,
    BrightBlue,
    BrightGreen,   // "Matrix" mode — G+
    BrightCyan,
    BrightRed,     // R+
    BrightMagenta,
    Yellow,        // Amber CRT — GR+ / W+
    White          // W+
};

// Parse classic xBase color strings: "W+", "G*", "RB/N", "YELLOW", etc.
Theme       parseTheme(const std::string& s);
std::string themeName(Theme t);
void        applyTheme(Theme t);
Theme       currentTheme();

// Temporary emitters (do NOT change global theme)
void        emitTheme(Theme t);
void        emitCurrentTheme();

// Tree/browser rotating color helpers
std::size_t treePaletteSize();
Theme       treeThemeForLevel(int depth);

// Tree color toggle
bool        treeColorEnabled();
void        setTreeColorEnabled(bool enabled);

} // namespace colors
} // namespace dli
