#include <clocale>

#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
#endif

#include "runtime/utf8_init.hpp"

namespace dottalk {
  void init_utf8() {
  #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setlocale(LC_ALL, ".UTF8");
  #else
    std::setlocale(LC_ALL, "");
  #endif
  }
}
