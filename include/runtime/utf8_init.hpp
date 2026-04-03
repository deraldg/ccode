#pragma once

// Header-only UTF-8/VT init for DotTalk++
// - Keeps narrow std::cout (UTF-8 bytes)
// - VT OUTPUT is toggleable (colors/clears)  <-- performance knob
// - DOES NOT enable VT INPUT (breaks Turbo Vision / CLI)
// - Sets a sane UTF-8 locale elsewhere, without throwing.

#include <locale>

#if defined(_WIN32)
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
  #include <io.h>
  #include <fcntl.h>
#endif

// ------------------------------------------------------------
// TEMP PERFORMANCE SWITCH
// 0 = VT OUTPUT OFF (FAST; ANSI colors/clears disabled)
// 1 = VT OUTPUT ON  (ANSI colors/clears enabled)
// ------------------------------------------------------------
#ifndef DOTTALK_ENABLE_VT_OUTPUT
#define DOTTALK_ENABLE_VT_OUTPUT 0
#endif

namespace dottalk {

inline void init_utf8() {
#if defined(_WIN32)
    // 1) Switch console code pages to UTF-8.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 2) VT OUTPUT (ANSI) — optional (this is the slowdown knob).
    if (HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
#if DOTTALK_ENABLE_VT_OUTPUT
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
#else
            // Force VT output OFF (fast path)
            mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
#endif
            SetConsoleMode(hOut, mode);
        }
    }

    // 3) ENSURE VT INPUT IS OFF (critical)
    if (HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        hIn != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hIn, &mode)) {
            mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
            SetConsoleMode(hIn, mode);
        }
    }

    // 4) Keep narrow I/O; just ensure text mode.
    _setmode(_fileno(stdout), _O_TEXT);
    _setmode(_fileno(stderr), _O_TEXT);

    // 5) Adopt system locale (don’t fail if unavailable).
    try { std::locale::global(std::locale("")); } catch (...) {}
#else
    try {
        std::locale::global(std::locale("C.UTF-8"));
    } catch (...) {
        try { std::locale::global(std::locale("en_US.UTF-8")); } catch (...) {}
    }
#endif
}

} // namespace dottalk
