#include <iostream>
#include <sstream>
#include <string>
#include <cctype>

#include "xbase.hpp"
#include "xbase_locks.hpp"
#include "textio.hpp"

static std::string read_rest(std::istringstream& iss) {
    std::string s; std::getline(iss >> std::ws, s); return textio::trim(s);
}
static std::string up(std::string s) { for (auto& c: s) c = (char)std::toupper((unsigned char)c); return s; }

void cmd_UNLOCK(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "UNLOCK: no table open.\n"; return; }

    std::string rest = up(read_rest(iss));
    if (rest.empty()) {
        // UNLOCK current record
        const auto rn = static_cast<uint32_t>(a.recno());
        if (rn == 0) { std::cout << "UNLOCK: no current record.\n"; return; }
        xbase::locks::unlock_record(a, rn);
        std::cout << "UNLOCK: record " << rn << " unlocked.\n";
        return;
    }

    if (rest == "ALL" || rest == "TABLE") {
        xbase::locks::unlock_table(a);
        std::cout << "UNLOCK: table unlocked.\n";
        return;
    }

    // UNLOCK <n>
    try {
        uint32_t n = static_cast<uint32_t>(std::stoul(rest));
        xbase::locks::unlock_record(a, n);
        std::cout << "UNLOCK: record " << n << " unlocked.\n";
    } catch (...) {
        std::cout << "Usage: UNLOCK [<recno>|ALL]\n";
    }
}



