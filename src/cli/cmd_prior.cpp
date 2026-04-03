// src/cli/cmd_prior.cpp
// PRIOR = previous visible record in current logical view
// (active order + filter visibility).

#include <sstream>
#include <iostream>
#include <cstdint>

#include "xbase.hpp"
#include "cli/logical_nav.hpp"
#include "cli/settings.hpp"

void cmd_PRIOR(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) {
        std::cout << "PRIOR: no file open.\n";
        return;
    }

    const int32_t rn = cli::logical_nav::prev_recno(A, A.recno());
    if (rn <= 0) {
        std::cout << "PRIOR: at top.\n";
        return;
    }

    if (!A.gotoRec(rn) || !A.readCurrent()) {
        std::cout << "PRIOR: failed.\n";
        return;
    }

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
}