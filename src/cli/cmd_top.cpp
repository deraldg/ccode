#include <iostream>
#include <sstream>
#include "xbase.hpp"

void cmd_TOP(xbase::DbArea& a, std::istringstream& iss) {
    (void)iss;
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    if (!a.top())    { std::cout << "Failed to go TOP\n"; return; }
    std::cout << "Recno: " << a.recno() << "\n";
}
