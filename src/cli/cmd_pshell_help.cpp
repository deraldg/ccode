// cmd_pshell_help.cpp

#include "xbase.hpp"
#include <sstream>
#include <string>

// Forward declaration from cmd_pshell.cpp
extern void show_pshell_help(const std::string& arg);

void cmd_PSHELL(xbase::DbArea& /*area*/, std::istringstream& iss) {
    std::string args;
    std::getline(iss >> std::ws, args);   // everything after "PSHELL"
    show_pshell_help(args);
}

void cmd_PS(xbase::DbArea& area, std::istringstream& iss) {
    cmd_PSHELL(area, iss);   // alias
}