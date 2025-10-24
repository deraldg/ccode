// src/cli/cmd_clear.cpp
// DotTalk++ — CLEAR (and CLS alias via registry)
// Cross-platform default: Windows -> cls; others -> ANSI escape.
// We keep it minimal and non-invasive (no engine touches).

#include <iostream>
#include <sstream>
#include <cstdlib>

#include "xbase.hpp"

void cmd_CLEAR(xbase::DbArea& /*a*/, std::istringstream& /*iss*/) {
#ifdef _WIN32
    // Windows console: use built-in cls for predictable behavior in CMD/PowerShell.
    std::system("cls");
#else
    // ANSI-capable terminals (Linux/macOS/Windows 10+): clear + home.
    std::cout << "\x1b[2J\x1b[H";
    std::cout.flush();
#endif
}
