#include <sstream>
#include <iostream>
#include "xbase.hpp"
#include "cli/settings.hpp"

using cli::Settings;

void cmd_SKIP(xbase::DbArea& A, std::istringstream& in)
{
    if (!A.isOpen()) { std::cout << "SKIP: no file open.\n"; return; }

    int n = 1;
    if (!(in >> n)) { in.clear(); } // default +1

    const bool hide_deleted = Settings::instance().deleted_on.load(); // ON means HIDE
    int steps = (n >= 0 ? n : -n);
    int dir   = (n >= 0 ? +1 : -1);

    bool ok = true;
    while (steps-- > 0) {
        if (!(ok = A.skip(dir))) break;
        if (hide_deleted) {
            while (ok && A.isDeleted()) ok = A.skip(dir);
            if (!ok) break;
        }
    }
    if (!ok) std::cout << "SKIP: hit boundary.\n";
}
