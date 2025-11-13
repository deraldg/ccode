#include <sstream>
#include <iostream>
#include "xbase.hpp"
#include "cli/settings.hpp"

using cli::Settings;

void cmd_TOP(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) { std::cout << "TOP: no file open.\n"; return; }
    if (!A.top())    { std::cout << "TOP: failed.\n"; return; }
    if (Settings::instance().talk_on.load())
        std::cout << "Recno: " << A.recno() << "\n";
}
