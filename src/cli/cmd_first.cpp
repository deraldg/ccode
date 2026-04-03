// src/cli/cmd_first.cpp
// FIRST = first visible record in current logical view
// (active order + filter visibility).

#include <sstream>
#include <iostream>
#include <cstdint>

#include "xbase.hpp"
#include "cli/logical_nav.hpp"
#include "cli/settings.hpp"

void cmd_FIRST(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) {
        std::cout << "FIRST: no file open.\n";
        return;
    }

    const int32_t rn = cli::logical_nav::first_recno(A);
    if (rn <= 0) {
        std::cout << "FIRST: failed.\n";
        return;
    }

    if (!A.gotoRec(rn) || !A.readCurrent()) {
        std::cout << "FIRST: failed.\n";
        return;
    }

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
}