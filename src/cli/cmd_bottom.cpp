#include <sstream>
#include <iostream>
#include <cstdint>

#include "xbase.hpp"
#include "cli/nav_select.hpp"
#include "cli/settings.hpp"

void cmd_BOTTOM(xbase::DbArea& A, std::istringstream&)
{
    if (!A.isOpen()) {
        std::cout << "BOTTOM: no file open.\n";
        return;
    }

    const int32_t rn = cli::navsel::pick_recno(
        A,
        cli::navsel::Mode::AutoByFilter,
        cli::navsel::Step::Last);

    if (rn <= 0 || !A.gotoRec(rn) || !A.readCurrent()) {
        std::cout << "BOTTOM: failed.\n";
        return;
    }

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
}
