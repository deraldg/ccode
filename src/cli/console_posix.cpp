#ifndef _WIN32
#include "cli/console.hpp"
#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

// ====== Global style knobs (POSIX) =========================================
static constexpr bool kUseAsciiFrame = false;

static const char* kReset        = "\x1b[0m";
static const char* kFrameStyle   = "\x1b[30;43m";   // black on amber
static const char* kHeaderStyle  = "\x1b[1;32m";    // bold green (for callers)
// Frame glyphs
//static const char* kTL = kUseAsciiFrame ? "+" : u8"?";
//static const char* kTR = kUseAsciiFrame ? "+" : u8"?";
//static const char* kBL = kUseAsciiFrame ? "+" : u8"?";
//static const char* kBR = kUseAsciiFrame ? "+" : u8"?";
//static const char* kHZ = kUseAsciiFrame ? "-" : u8"?";
//static const char* kVT = kUseAsciiFrame ? "|" : u8"?";

//Linux...remove the u8
static const char* kTL = kUseAsciiFrame ? "+" : "?";
static const char* kTR = kUseAsciiFrame ? "+" : "?";
static const char* kBL = kUseAsciiFrame ? "+" : "?";
static const char* kBR = kUseAsciiFrame ? "+" : "?";
static const char* kHZ = kUseAsciiFrame ? "-" : "?";
static const char* kVT = kUseAsciiFrame ? "|" : "?";
// ==========================================================================

struct PosixConsole : Console {
    termios orig{};
    PosixConsole() {
        tcgetattr(STDIN_FILENO, &orig);
        termios raw = orig;
        raw.c_lflag &= ~(ICANON | ECHO);   // raw-ish mode: no line buffering, no echo
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        // stdout should already be UTF-8 on most systems; box glyphs print fine.
    }
    ~PosixConsole() {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    }

    void clear() override { std::cout << "\x1b[2J\x1b[H"; }
    void move_to(int x,int y) override { std::cout << "\x1b[" << (y+1) << ";" << (x+1) << "H"; }

    void draw_text(int x,int y,const std::string&s,int clip=-1) override {
        move_to(x,y);
        if (clip>=0 && (int)s.size()>clip) std::cout << s.substr(0,(size_t)clip);
        else std::cout << s;
    }

    void draw_frame(int l,int t,int w,int h) override {
        if (w<2 || h<2) return;

        // Corners
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

    int get_key() override {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1) return (int)c;
        return -1;
    }

    Size size() override {
        Size s{80,25};
        struct winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            if (ws.ws_col) s.cols = ws.ws_col;
            if (ws.ws_row) s.rows = ws.ws_row;
        }
        return s;
    }
};

Console* make_console() { return new PosixConsole(); }
#endif // !_WIN32



