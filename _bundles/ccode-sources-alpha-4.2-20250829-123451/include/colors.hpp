// include/colors.hpp
#pragma once
#include <iostream>

namespace colors {

enum class Theme { Green, Amber, Default };

inline void applyTheme(Theme t) {
    switch (t) {
    case Theme::Green:  std::cout << "\033[32m"; break;
    case Theme::Amber:  std::cout << "\033[33m"; break;
    case Theme::Default: default: std::cout << "\033[0m"; break;
    }
}

inline void reset() {
    std::cout << "\033[0m";
}

} // namespace colors
