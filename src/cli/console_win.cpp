#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "cli/console.hpp"
#include <iostream>
#include <conio.h>
#include <windows.h>

// ====== Global style knobs (Windows) =======================================
// Toggle to force ASCII frame if your font or environment has trouble with Unicode.
static constexpr bool kUseAsciiFrame = false;

// Colors/styles (ANSI SGR)
static const char* kReset        = "\x1b[0m";
static const char* kFrameStyle   = "\x1b[30;43m";   // black fg (30) on amber/yellow bg (43)
static const char* kHeaderStyle  = "\x1b[1;32m";    // bold green (example; header text set by caller)

// Frame glyphs
static const char* kTL = kUseAsciiFrame ? "+" : "┌";
static const char* kTR = kUseAsciiFrame ? "+" : "┐";
static const char* kBL = kUseAsciiFrame ? "+" : "└";
static const char* kBR = kUseAsciiFrame ? "+" : "┘";
static const char* kHZ = kUseAsciiFrame ? "-" : "─";
static const char* kVT = kUseAsciiFrame ? "|" : "│";
// ==========================================================================

struct WinConsole : Console {
    WinConsole() {
        // Enable ANSI VT processing so cursor moves & colors work
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(h, &mode)) {
                SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
        // UTF-8 out (helps with box glyphs)
        SetConsoleOutputCP(CP_UTF8);
    }

    void clear() override { std::cout << "\x1b[2J\x1b[H"; }

    void move_to(int x,int y) override {
        std::cout << "\x1b[" << (y+1) << ";" << (x+1) << "H";
    }

    void draw_text(int x,int y,const std::string&s,int clip=-1) override {
        move_to(x,y);
        if (clip>=0 && (int)s.size()>clip) std::cout << s.substr(0,(size_t)clip);
        else std::cout << s;
    }

    void draw_frame(int l,int t,int w,int h) override {
        if (w<2 || h<2) return;

        // Corners (amber bg)
        draw_text(l,       t,       std::string(kFrameStyle) + kTL + kReset);
        draw_text(l+w-1,   t,       std::string(kFrameStyle) + kTR + kReset);
        draw_text(l,       t+h-1,   std::string(kFrameStyle) + kBL + kReset);
        draw_text(l+w-1,   t+h-1,   std::string(kFrameStyle) + kBR + kReset);

        // Horizontal edges
        for (int i=1;i<w-1;i++){
            draw_text(l+i, t,       std::string(kFrameStyle) + kHZ + kReset);
            draw_text(l+i, t+h-1,   std::string(kFrameStyle) + kHZ + kReset);
        }
        // Vertical edges
        for (int y=t+1;y<t+h-1;y++){
            draw_text(l,     y,     std::string(kFrameStyle) + kVT + kReset);
            draw_text(l+w-1, y,     std::string(kFrameStyle) + kVT + kReset);
        }
    }

    int get_key() override { return _getch(); }

    Size size() override {
        Size s{80,25};
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
            s.cols = info.srWindow.Right  - info.srWindow.Left + 1;
            s.rows = info.srWindow.Bottom - info.srWindow.Top  + 1;
        }
        return s;
    }
};

Console* make_console() { return new WinConsole(); }
#endif // _WIN32
