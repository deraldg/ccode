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
        std::cout << "COLOR is " << themeName(currentTheme()) << "\n";
        std::cout << "TREECOLOR is " << (treeColorEnabled() ? "ON" : "OFF") << "\n";
        std::cout << "TREE palette rotates across " << treePaletteSize() << " levels:\n";
        for (std::size_t i = 0; i < treePaletteSize(); ++i) {
            const Theme t = treeThemeForLevel(static_cast<int>(i));
            emitTheme(t);
            std::cout << "  Level " << i << ": " << themeName(t) << "\n";
            emitCurrentTheme();
        }
        return;
    }

    std::string sub;
    if (arg == "TREE" || arg == "TREECOLOR") {
        if (!(iss >> sub)) {
            std::cout << "TREECOLOR is " << (treeColorEnabled() ? "ON" : "OFF") << "\n";
            return;
        }

        for (char& ch : sub) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

        if (sub == "ON") {
            setTreeColorEnabled(true);
            std::cout << "TREECOLOR set to ON\n";
            return;
        }
        if (sub == "OFF") {
            setTreeColorEnabled(false);
            std::cout << "TREECOLOR set to OFF\n";
            return;
        }

        std::cout << "Usage: COLOR TREE ON|OFF\n";
        return;
    }

    // Accept COLOR DEFAULT | GREEN | AMBER | etc.
    Theme t = parseTheme(arg);
    applyTheme(t);
    std::cout << "COLOR set to " << themeName(t) << "\n";
}
