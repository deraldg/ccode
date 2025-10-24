#include "console_utils.hpp"
#if defined(_WIN32)
  #include <windows.h>
  #include <conio.h>
#endif
#include <iostream>

int console_width() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return (csbi.srWindow.Right - csbi.srWindow.Left + 1);
    }
#endif
    return 80; // fallback
}

void press_any_key_blocking() {
#if defined(_WIN32)
    std::cout << "\n-- more -- (press any key)\n";
    _getch();
#else
    std::cout << "\n-- more -- (press ENTER)\n";
    std::cin.get();
#endif
}
