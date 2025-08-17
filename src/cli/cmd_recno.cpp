// src/cli/cmd_recno.cpp
#include <iostream>
#include <sstream>
#include "xbase.hpp"

void cmd_RECNO(xbase::DbArea& a, std::istringstream& iss) {
    (void)iss;
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    std::cout << a.recno() << "\n";
}
