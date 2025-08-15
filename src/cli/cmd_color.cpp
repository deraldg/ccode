#include <iostream>
#include <sstream>
#include <string>
#include "xbase.hpp"
#include "colors.hpp"
#include "textio.hpp"

void cmd_COLOR(xbase::DbArea&, std::istringstream& iss) {
    using namespace cli::colors;
    std::string arg;
    if (!(iss >> arg)) {
        std::cout << "Usage: COLOR <GREEN|AMBER|DEFAULT>" << std::endl;
        return;
    }
    bool ok=false;
    Theme t = parseTheme(arg, ok);
    if (!ok) {
        std::cout << "Unknown color. Use: COLOR GREEN | COLOR AMBER | COLOR DEFAULT" << std::endl;
        return;
    }
    applyTheme(t);
}
