#include "xbase.hpp"
#include <iostream>
#include <sstream>

void cmd_REFRESH(xbase::DbArea& a, std::istringstream&) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }
    const std::string fname = a.name();
    try {
        long keep = a.recno();
        a.open(fname);
        if (keep > 0 && keep <= a.recCount()) a.gotoRec(keep);
        std::cout << "Refreshed " << fname << " (" << a.recCount() << " records).\n";
    } catch (const std::exception& e) {
        std::cout << "Refresh failed: " << e.what() << "\n";
    }
}
