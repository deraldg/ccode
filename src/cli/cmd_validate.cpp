// src/cli/cmd_validate.cpp -- VALIDATE router
// Forwards subcommands like "VALIDATE UNIQUE ..." to their handlers.

#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>

#include "xbase.hpp"

void cmd_VALIDATE_UNIQUE(xbase::DbArea&, std::istringstream&);

static inline std::string upcopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
    return s;
}

void cmd_VALIDATE(xbase::DbArea& A, std::istringstream& in)
{
    std::streampos pos = in.tellg();
    std::string tok;
    if (!(in >> tok)) {
        std::cout << "Usage: VALIDATE UNIQUE FIELD <name> [IGNORE DELETED] [REPAIR] [REPORT TO <path>]\n";
        return;
    }
    const std::string U = upcopy(tok);

    in.clear();
    in.seekg(pos);

    if (U == "UNIQUE") {
        cmd_VALIDATE_UNIQUE(A, in);
        return;
    }

    std::cout << "VALIDATE: unknown subcommand '" << tok
              << "'. Try: VALIDATE UNIQUE FIELD <name> [...]\n";
}