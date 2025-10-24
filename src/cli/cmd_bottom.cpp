#include <iostream>
#include <sstream>
#include "xbase.hpp"

void cmd_BOTTOM(xbase::DbArea& a, std::istringstream& iss) {
    (void)iss;
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    if (!a.bottom()) { std::cout << "Failed to go BOTTOM\n"; return; }
    std::cout << "Recno: " << a.recno() << "\n";
}
