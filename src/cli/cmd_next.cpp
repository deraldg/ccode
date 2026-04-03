// src/cli/cmd_next.cpp
// NEXT = next visible record in current logical view
// (active order + filter visibility).

#include <sstream>
#include <iostream>
#include <cstdint>

#include "xbase.hpp"
#include "cli/logical_nav.hpp"
#include "cli/settings.hpp"

void cmd_NEXT(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) {
        std::cout << "NEXT: no file open.\n";
        return;
    }

    const int32_t rn = cli::logical_nav::next_recno(A, A.recno());
    if (rn <= 0) {
        std::cout << "NEXT: at end.\n";
        return;
    }

    if (!A.gotoRec(rn) || !A.readCurrent()) {
        std::cout << "NEXT: failed.\n";
        return;
    }

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
}