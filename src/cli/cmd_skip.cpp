#include <sstream>
#include <iostream>
#include <cstdint>

#include "xbase.hpp"
#include "cli/nav_select.hpp"
#include "cli/settings.hpp"

void cmd_SKIP(xbase::DbArea& A, std::istringstream& in)
{
    if (!A.isOpen()) {
        std::cout << "SKIP: no file open.\n";
        return;
    }

    int n = 1;
    if (!(in >> n)) {
        n = 1;
    }

    const bool talk = cli::Settings::instance().talk_on.load();

    if (n == 0) {
        if (!A.readCurrent()) {
            std::cout << "SKIP: failed to read record.\n";
            return;
        }
        if (talk) std::cout << "Recno: " << A.recno() << "\n";
        return;
    }

    int steps = (n >= 0 ? n : -n);
    const auto step_kind = (n >= 0)
        ? cli::navsel::Step::Next
        : cli::navsel::Step::Prior;

    int32_t current = A.recno();
    int32_t rn = 0;
    bool moved = false;

    while (steps-- > 0) {
        rn = cli::navsel::pick_recno(
            A,
            cli::navsel::Mode::AutoByFilter,
            step_kind,
            current);

        if (rn <= 0) {
            if (!moved) {
                std::cout << "SKIP: at end.\n";
                return;
            }
            break;
        }

        current = rn;
        moved = true;
    }

    if (!moved || !A.gotoRec(current) || !A.readCurrent()) {
        std::cout << "SKIP: failed.\n";
        return;
    }

    if (talk) std::cout << "Recno: " << A.recno() << "\n";
}
