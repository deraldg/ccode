// src/cli/cmd_setorder.cpp — SETORDER <0|stem|path.inx>
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
static std::string read_arg(std::istringstream& args) {
    std::string a;
    if (!(args >> a)) return {};
    if (to_upper(a) == "TO") {
        if (!(args >> a)) return {};
    }
    return a;
}

void cmd_SETORDER(xbase::DbArea& A, std::istringstream& args)
{
    std::string arg = read_arg(args);
    if (arg.empty()) {
        std::cout << "SET ORDER: missing argument. Use 0, a tag stem, or a .inx path.\n";
        return;
    }

    // 0 => physical
    if (arg == "0") {
        orderstate::clearOrder(A);
        std::cout << "SET ORDER: physical order (cleared).\n";
        return;
    }

    // If explicit .CNX, steer user to container flow.
    if (arg.size() > 4 && to_upper(arg.substr(arg.size()-4)) == ".CNX") {
        std::cout << "SET ORDER: '" << arg
                  << "' is a CNX container; use SETCNX and then SETORDER <tag>.\n";
        return;
    }

    // If explicit .INX path
    if (arg.size() > 4 && to_upper(arg.substr(arg.size()-4)) == ".INX") {
        fs::path p = arg;
        orderstate::setOrder(A, p.string());
        std::cout << "SET ORDER: using index file '" << p.string() << "'.\n";
        return;
    }

    // Treat as a stem: try <stem>.inx (relative or absolute as typed later).
    {
        fs::path p = arg;
        p.replace_extension(".inx");
        orderstate::setOrder(A, p.string());
        std::cout << "SET ORDER: tried index file '" << p.string()
                  << "'. (If not found, ensure path/cwd.)\n";
        return;
    }
}
