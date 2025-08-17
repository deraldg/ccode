// src/cli/cmd_use.cpp
#include <iostream>
#include <sstream>
#include <fstream>
#include "xbase.hpp"
#include "textio.hpp"

using namespace xbase;

static bool file_exists(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return static_cast<bool>(in);
}

void cmd_USE(DbArea& a, std::istringstream& iss) {
    std::string db; iss >> db;
    if (db.empty()) { std::cout << "Usage: USE <dbf>\n"; return; }
    if (!textio::ends_with_ci(db, ".dbf")) db += ".dbf";

    if (!file_exists(db)) {
        std::cout << "Open failed: file not found\n";
        return;
    }

    try {
        a.open(db); // must not create if absent; DbArea::open should open existing
        std::cout << "Opened " << db << " with " << a.recCount() << " records.\n";
    } catch (const std::exception& e) {
        std::cout << "Open failed: " << e.what() << "\n";
    }
}
