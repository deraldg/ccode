// src/cli/cmd_last.cpp
// LAST = last visible record in current logical view
// (active order + filter visibility).

#include <sstream>
#include <iostream>
#include <cstdint>

#include "xbase.hpp"
#include "cli/logical_nav.hpp"
#include "cli/settings.hpp"

void cmd_LAST(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) {
        std::cout << "LAST: no file open.\n";
        return;
    }

    const int32_t rn = cli::logical_nav::last_recno(A);
    if (rn <= 0) {
        std::cout << "LAST: failed.\n";
        return;
    }

    if (!A.gotoRec(rn) || !A.readCurrent()) {
        std::cout << "LAST: failed.\n";
        return;
    }

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
}