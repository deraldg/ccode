#include <iostream>
#include <sstream>
#include "xbase.hpp"
#include "cli/settings.hpp"

using cli::Settings;

void cmd_GOTO(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    int n = 0; iss >> n;
    if (!iss || n < 1 || n > a.recCount()) {
        std::cout << "Usage: GOTO <1.." << a.recCount() << ">\n"; return;
    }
    if (!a.gotoRec(n)) { std::cout << "Goto failed\n"; return; }
    if (Settings::instance().talk_on.load())
        std::cout << "Recno: " << a.recno() << "\n";
}
