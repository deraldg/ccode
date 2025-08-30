// src/cli/cmd_color.cpp
#include "xbase.hpp"
#include "colors.hpp"
#include <iostream>
#include <sstream>
#include <string>

using xbase::DbArea;

void cmd_COLOR(DbArea& /*A*/, std::istringstream& iss) {
    using namespace dli::colors;

    std::string arg;
    if (!(iss >> arg)) {
        // No arg: show current theme
        std::cout << "COLOR is " << themeName(currentTheme()) << "\n";
        return;
    }

    // Accept COLOR DEFAULT | GREEN | AMBER
    Theme t = parseTheme(arg);
    applyTheme(t);
    std::cout << "COLOR set to " << themeName(t) << "\n";
}
