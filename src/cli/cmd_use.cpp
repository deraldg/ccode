#include <iostream>
#include <sstream>
#include "xbase.hpp"
#include "textio.hpp"

using namespace xbase;

void cmd_USE(DbArea& a, std::istringstream& iss) {
    std::string db; iss >> db;
    if (db.empty()) { std::cout << "Usage: USE <dbf>\n"; return; }
    if (!textio::ends_with_ci(db, ".dbf")) db += ".dbf";
    try {
        a.open(db);
        std::cout << "Opened " << db << " with " << a.recCount() << " records.\n";
    } catch (const std::exception& e) {
        std::cout << "Open failed: " << e.what() << "\n";
    }
}
