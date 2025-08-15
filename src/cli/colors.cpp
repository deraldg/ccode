#include "colors.hpp"
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <string>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#endif

namespace cli { namespace colors {

static Theme g_current = Theme::Default;

// Safe getenv -> std::string
static std::string env_or_empty(const char* name) {
#if defined(_MSC_VER)
    char* buf = nullptr; size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) == 0 && buf) { std::string s(buf); free(buf); return s; }
    return {};
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string{};
#endif
}

static bool no_color_requested() {
    // https://no-color.org/
    return !env_or_empty("NO_COLOR").empty();
}

bool enableAnsi() {
    if (no_color_requested()) return false;

#ifdef _WIN32
    // Try to enable VT sequences on stdout and stderr
    auto enable_on = [](DWORD std_handle) -> bool {
        HANDLE h = GetStdHandle(std_handle);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(h, &mode)) return false;
        DWORD want = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (want == mode) return true; // already enabled
        return SetConsoleMode(h, want) != 0;
    };
    bool ok_out = enable_on(STD_OUTPUT_HANDLE);
    bool ok_err = enable_on(STD_ERROR_HANDLE);
    return ok_out || ok_err;
#else
    // On POSIX terminals assume ANSI is fine unless NO_COLOR is set.
    return true;
#endif
}

bool has256() {
    if (!env_or_empty("CLICOLOR_FORCE").empty()) return true;
    const std::string term = env_or_empty("TERM");
    const std::string colorterm = env_or_empty("COLORTERM");
    // Common signals for 256/truecolor
    if (term.find("256color") != std::string::npos) return true;
    if (colorterm.find("truecolor") != std::string::npos) return true;
    if (colorterm.find("24bit") != std::string::npos) return true;
#ifdef _WIN32
    // Windows Terminal / modern consoles typically support at least 256 once VT is on
    if (!env_or_empty("WT_SESSION").empty()) return true;
    if (!env_or_empty("ConEmuANSI").empty()) return true;
#endif
    return false;
}

std::string seqReset() {
    return "\x1b[0m";
}

std::string seqTheme(Theme t, bool use256) {
    if (t == Theme::Default) return seqReset();
    if (use256) {
        // 256-color approximations
        // Green: 46, Amber/Orange: 214
        if (t == Theme::Green) return "\x1b[38;5;46m";
        if (t == Theme::Amber) return "\x1b[38;5;214m";
    }
    // Basic 8-color fallback
    if (t == Theme::Green) return "\x1b[32m";
    if (t == Theme::Amber) return "\x1b[33m";
    return seqReset();
}

void applyTheme(Theme t) {
    if (!enableAnsi()) return;
    if (t == Theme::Default) {
        std::cout << seqReset();
        g_current = Theme::Default;
        return;
    }
    std::cout << seqTheme(t, has256());
    g_current = t;
}

void reset() {
    if (!enableAnsi()) return;
    std::cout << seqReset();
    g_current = Theme::Default;
}

Theme parseTheme(const std::string& s, bool& ok) {
    ok = true;
    std::string u; u.reserve(s.size());
    for (char c : s) u.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    if (u == "GREEN")   return Theme::Green;
    if (u == "AMBER" || u == "ORANGE") return Theme::Amber;
    if (u == "DEFAULT" || u == "RESET") return Theme::Default;
    ok = false;
    return Theme::Default;
}

}} // namespace cli::colors
