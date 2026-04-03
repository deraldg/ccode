#include <sstream>
#include <iostream>
#include <cstdint>

#include "xbase.hpp"
#include "cli/nav_select.hpp"
#include "cli/settings.hpp"

void cmd_TOP(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) {
        std::cout << "TOP: no file open.\n";
        return;
    }

    const int32_t rn = cli::navsel::pick_recno(
        A,
        cli::navsel::Mode::AutoByFilter,
        cli::navsel::Step::First);

    if (rn <= 0 || !A.gotoRec(rn) || !A.readCurrent()) {
        std::cout << "TOP: failed.\n";
        return;
    }

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
}
