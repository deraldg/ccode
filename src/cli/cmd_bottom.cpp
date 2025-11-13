#include <sstream>
#include <iostream>
#include "xbase.hpp"
#include "cli/settings.hpp"

using cli::Settings;

void cmd_BOTTOM(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) { std::cout << "BOTTOM: no file open.\n"; return; }
    if (!A.bottom()) { std::cout << "BOTTOM: failed.\n"; return; }
    if (Settings::instance().talk_on.load())
        std::cout << "Recno: " << A.recno() << "\n";
}
