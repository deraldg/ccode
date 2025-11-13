// SET UNIQUE FIELD <name> ON|OFF
// Lists current unique fields if called without args.

#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/unique_registry.hpp"

using namespace textio;

static inline std::string upcopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

void cmd_SET_UNIQUE(xbase::DbArea& A, std::istringstream& in) {
    std::string tok1;
    if (!(in >> tok1)) {
        auto fields = unique_reg::list_unique_fields(A);
        if (fields.empty()) { std::cout << "UNIQUE: (none)\n"; return; }
        std::cout << "UNIQUE fields: ";
        for (size_t i=0;i<fields.size();++i) {
            if (i) std::cout << ", ";
            std::cout << fields[i];
        }
        std::cout << "\n";
        return;
    }

    const std::string U1 = upcopy(tok1);
    if (U1 != "FIELD") {
        std::cout << "Usage: SET UNIQUE FIELD <name> ON|OFF\n";
        return;
    }

    std::string fname, onoff;
    if (!(in >> fname >> onoff)) {
        std::cout << "Usage: SET UNIQUE FIELD <name> ON|OFF\n";
        return;
    }

    const std::string Uon = upcopy(onoff);
    if (Uon != "ON" && Uon != "OFF") {
        std::cout << "Usage: SET UNIQUE FIELD <name> ON|OFF\n";
        return;
    }

    unique_reg::set_unique_field(A, fname, Uon == "ON");
    std::cout << "UNIQUE " << (Uon == "ON" ? "ON" : "OFF")
              << " for FIELD " << upcopy(fname) << ".\n";
}
