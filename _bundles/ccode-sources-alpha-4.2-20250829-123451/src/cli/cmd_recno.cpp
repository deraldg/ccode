// src/cli/cmd_recno.cpp
#include "xbase.hpp"
#include "textio.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <cctype>
#include <limits>
#include <cstdlib>

static bool parse_int32(const std::string& s, int32_t& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (*end != '\0') return false;
    if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max()) return false;
    out = static_cast<int32_t>(v);
    return true;
}

void cmd_RECNO(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    std::string tok;
    if (!(iss >> tok)) {
        // no args: print current recno
        std::cout << a.recno() << "\n";
        return;
    }

    int32_t n = 0;
    if (!parse_int32(tok, n)) { std::cout << "Usage: RECNO [n]\n"; return; }

    if (n < 1 || n > a.recCount()) {
        std::cout << "Record number out of range (1.." << a.recCount() << ").\n";
        return;
    }

    if (!a.gotoRec(n) || !a.readCurrent()) {
        std::cout << "Unable to navigate to record " << n << ".\n";
        return;
    }

    std::cout << a.recno() << "\n";
}
