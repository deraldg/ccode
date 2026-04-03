// foxtalk_palette.cpp — minimal, TV-agnostic shim for theme selection
// This compiles cleanly with magiblot/tvision v4 and provides the API
// used by cmd_foxtalk.cpp. We’ll add real TV palette wiring later.

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace {

inline std::string ucase(std::string s) {
    for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Canonical theme list in rotation order.
static std::vector<std::string> kThemes{
    "NEON",     // keep your current bright scheme as-is
    "FOXPRO",   // classic blue/gray target
    "DBASE",    // classic cyan/blue target
    "CLIPPER"   // amber/mono target
};

// Persisted current theme name.
static std::string g_current = "NEON";

// Find theme index (case-insensitive). Returns -1 if not found.
int findTheme(const std::string &name) {
    const auto up = ucase(name);
    for (int i = 0; i < (int)kThemes.size(); ++i)
        if (ucase(kThemes[i]) == up) return i;
    return -1;
}

} // namespace

namespace foxtalk {

// Set the active theme name. (No-ops for actual colors for now.)
void applyRetroPalette(const std::string &name) {
    if (name.empty()) {
        // keep current on empty set
        return;
    }
    const int idx = findTheme(name);
    if (idx >= 0) {
        g_current = kThemes[(size_t)idx];
        // NOTE: Real TV palette application will be added later:
        // - override getPalette() in key views/windows
        // - return different palette strings per theme
        // - optionally repaint desktop
    } else {
        // Unknown name: keep current (no throw so Foxtalk stays friendly)
    }
}

std::string currentPaletteName() {
    return g_current;
}

std::string nextPaletteName(const std::string &cur) {
    int idx = findTheme(cur.empty() ? g_current : cur);
    if (idx < 0) {
        // If unknown, snap to first
        return kThemes.front();
    }
    idx = (idx + 1) % (int)kThemes.size();
    return kThemes[(size_t)idx];
}

} // namespace foxtalk
