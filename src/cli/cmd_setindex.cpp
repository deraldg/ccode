// src/cli/cmd_setindex.cpp — SET INDEX TO <file.inx[,more...]>
// CNX note (alpha): do NOT open .cnx via SET INDEX.
// Use: SETCNX [<table|path.cnx>] then SETORDER <tag>.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "order_state.hpp"

namespace fs = std::filesystem;

static std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return char(std::toupper(c)); });
    return s;
}

void cmd_SETINDEX(xbase::DbArea& A, std::istringstream& args)
{
    std::string tok;
    if (!(args >> tok)) {
        std::cout << "SET INDEX: missing filename.\n";
        return;
    }
    if (to_upper(tok) == "TO") {
        if (!(args >> tok)) {
            std::cout << "SET INDEX: missing filename.\n";
            return;
        }
    }

    // Warn off CNX here.
    if (tok.size() > 4 && to_upper(tok.substr(tok.size()-4)) == ".CNX") {
        std::cout << "SET INDEX: '" << tok
                  << "' looks like a CNX container. Use SETCNX and SETORDER instead.\n";
        return;
    }

    fs::path p = tok;
    if (!p.has_extension()) p.replace_extension(".inx");

    orderstate::setOrder(A, p.string());
    std::cout << "Index set: " << p.filename().string() << "\n";
}
