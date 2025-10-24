// src/cli/colors.hpp
#pragma once
#include <string>

namespace dli {
namespace colors {

enum class Theme { Default, Green, Amber };

Theme       parseTheme(const std::string& s);
std::string themeName(Theme t);
void        applyTheme(Theme t);
Theme       currentTheme();

} // namespace colors
} // namespace dli
