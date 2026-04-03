#include "dli/screen.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dli {

static int gW = 0, gH = 0;
static bool gVT = false;
static
#ifdef _WIN32
HANDLE
#else
int
#endif
hOut = 0;

static std::vector<std::string> gShadow;

// ====================== HELPERS ======================
#ifdef _WIN32
static void write_at(int x, int y, const char* data, size_t n) {
    if (!hOut || !data || n == 0) return;
    DWORD dummy = 0;
    COORD pos{(SHORT)x, (SHORT)y};
    SetConsoleCursorPosition(hOut, pos);
    WriteFile(hOut, data, (DWORD)n, &dummy, nullptr);
}
#else
static void ansi_move_to(int x, int y) {
    std::printf("\x1b[%d;%dH", y + 1, x + 1);
}

static void ansi_write_at(int x, int y, const char* data, size_t n) {
    if (!data || n == 0) return;
    ansi_move_to(x, y);
    std::fwrite(data, 1, n, stdout);
}
#endif

// ====================== CORE ======================
int screen_width()  { return gW; }
int screen_height() { return gH; }
const std::vector<std::string>& screen_shadow() { return gShadow; }

bool screen_enable_vt(bool enable) {
#ifdef _WIN32
    if (!hOut) hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) return gVT;
    if (enable) mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    else        mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, mode)) gVT = enable;
    return gVT;
#else
    (void)enable;
    gVT = true;
    return gVT;
#endif
}

void screen_init(int width, int height) {
    gW = std::max(1, width);
    gH = std::max(1, height);
    gShadow.assign(gH, std::string(gW, ' '));

#ifdef _WIN32
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    CONSOLE_CURSOR_INFO ci{25, FALSE};
    SetConsoleCursorInfo(hOut, &ci);
    screen_enable_vt(true);
#else
    hOut = 1;
    gVT = true;
    std::printf("\x1b[?25l");
    std::fflush(stdout);
#endif
}

void screen_shutdown() {
#ifdef _WIN32
    if (hOut) {
        CONSOLE_CURSOR_INFO ci{25, TRUE};
        SetConsoleCursorInfo(hOut, &ci);
    }
#else
    std::printf("\x1b[0m\x1b[?25h");
    std::fflush(stdout);
#endif
    gShadow.clear();
    gW = gH = 0;
}

void screen_clear(bool clear_console) {
    for (auto& line : gShadow) std::fill(line.begin(), line.end(), ' ');

    if (!clear_console) return;

#ifdef _WIN32
    if (hOut) {
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        GetConsoleScreenBufferInfo(hOut, &csbi);
        DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
        DWORD written = 0;
        COORD home{0,0};
        FillConsoleOutputCharacterA(hOut, ' ', cells, home, &written);
        FillConsoleOutputAttribute(hOut, csbi.wAttributes, cells, home, &written);
        SetConsoleCursorPosition(hOut, home);
    }
#else
    std::printf("\x1b[2J\x1b[H");
    std::fflush(stdout);
#endif
}

// ====================== COLOR HELPERS ======================

std::string vt_reset(bool vt) {
    return vt ? "\x1b[0m" : "";
}

std::string vt_inverse(std::string_view s, bool vt) {
    if (!vt || s.empty()) return std::string(s);
    return "\x1b[7m" + std::string(s) + "\x1b[27m";
}

std::string vt_fg(std::string_view text, int fg_code, bool bold, bool vt) {
    if (!vt || text.empty()) return std::string(text);
    std::string s = "\x1b[" + std::to_string(fg_code) + "m";
    if (bold) s += "\x1b[1m";
    s.append(text.begin(), text.end());
    s += vt_reset(vt);
    return s;
}

// 256-color
std::string vt_256_fg(unsigned int id, bool vt) { return vt ? "\x1b[38;5;" + std::to_string(id) + "m" : ""; }
std::string vt_256_bg(unsigned int id, bool vt) { return vt ? "\x1b[48;5;" + std::to_string(id) + "m" : ""; }

std::string vt_fg_256(std::string_view text, unsigned int fg_id, bool bold, bool vt) {
    if (!vt || text.empty()) return std::string(text);
    std::string s = vt_256_fg(fg_id, vt);
    if (bold) s += "\x1b[1m";
    s.append(text.begin(), text.end());
    s += vt_reset(vt);
    return s;
}

std::string vt_fg_bg_256(std::string_view text, unsigned int fg_id, unsigned int bg_id, bool bold, bool vt) {
    if (!vt || text.empty()) return std::string(text);
    std::string s = vt_256_fg(fg_id, vt) + vt_256_bg(bg_id, vt);
    if (bold) s += "\x1b[1m";
    s.append(text.begin(), text.end());
    s += vt_reset(vt);
    return s;
}

// Truecolor RGB
std::string vt_rgb_fg(uint8_t r, uint8_t g, uint8_t b, bool vt) {
    return vt ? "\x1b[38;2;" + std::to_string(r) + ";" +
                       std::to_string(g) + ";" +
                       std::to_string(b) + "m" : "";
}

std::string vt_rgb_bg(uint8_t r, uint8_t g, uint8_t b, bool vt) {
    return vt ? "\x1b[48;2;" + std::to_string(r) + ";" +
                       std::to_string(g) + ";" +
                       std::to_string(b) + "m" : "";
}

std::string vt_fg_rgb(std::string_view text, uint8_t r, uint8_t g, uint8_t b, bool bold, bool vt) {
    if (!vt || text.empty()) return std::string(text);
    std::string s = vt_rgb_fg(r, g, b, vt);
    if (bold) s += "\x1b[1m";
    s.append(text.begin(), text.end());
    s += vt_reset(vt);
    return s;
}

std::string vt_fg_bg_rgb(std::string_view text,
                         uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
                         uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                         bool bold, bool vt) {
    if (!vt || text.empty()) return std::string(text);
    std::string s = vt_rgb_fg(fg_r, fg_g, fg_b, vt) +
                    vt_rgb_bg(bg_r, bg_g, bg_b, vt);
    if (bold) s += "\x1b[1m";
    s.append(text.begin(), text.end());
    s += vt_reset(vt);
    return s;
}

// ====================== WRITING ======================

static inline void pad_to_width(std::string& s) {
    if ((int)s.size() < gW) s.resize(gW, ' ');
    else if ((int)s.size() > gW) s.resize(gW);
}

void screen_write_line(int y, std::string_view text) {
    if (y < 0 || y >= gH) return;
    std::string line(text);
    pad_to_width(line);

#ifdef _WIN32
    int i = 0;
    const int n = (int)line.size();
    while (i < n) {
        while (i < n && line[i] == gShadow[y][i]) ++i;
        if (i >= n) break;
        const int start = i;
        while (i < n && line[i] != gShadow[y][i]) ++i;
        const int end = i;
        write_at(start, y, line.data() + start, (size_t)(end - start));
        std::copy(line.begin() + start, line.begin() + end, gShadow[y].begin() + start);
    }
#else
    ansi_write_at(0, y, line.data(), line.size());
    gShadow[y] = line;
    std::fflush(stdout);
#endif
}

void screen_write_span(int x, int y, std::string_view text) {
    if (y < 0 || y >= gH) return;
    if (x < 0 || x >= gW) return;

    int maxlen = gW - x;
    int n = (int)text.size();
    if (n > maxlen) n = maxlen;
    if (n <= 0) return;

#ifdef _WIN32
    write_at(x, y, text.data(), (size_t)n);
#else
    ansi_write_at(x, y, text.data(), (size_t)n);
    std::fflush(stdout);
#endif

    if (y >= 0 && y < (int)gShadow.size()) {
        if ((int)gShadow[y].size() < gW) gShadow[y].resize(gW, ' ');
        std::copy(text.begin(), text.begin() + n, gShadow[y].begin() + x);
    }
}

void screen_set_cursor(int x, int y, bool visible) {
#ifdef _WIN32
    if (hOut) {
        CONSOLE_CURSOR_INFO ci{25, visible ? TRUE : FALSE};
        SetConsoleCursorInfo(hOut, &ci);
        COORD pos{(SHORT)std::max(0, x), (SHORT)std::max(0, y)};
        SetConsoleCursorPosition(hOut, pos);
    }
#else
    std::printf(visible ? "\x1b[?25h" : "\x1b[?25l");
    std::printf("\x1b[%d;%dH", std::max(0, y) + 1, std::max(0, x) + 1);
    std::fflush(stdout);
#endif
}

} // namespace dli
