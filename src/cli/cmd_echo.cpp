// src/cli/cmd_echo.cpp
#include "xbase.hpp"
#include <sstream>
#include <iostream>

// ECHO just prints the rest of the line verbatim.
void cmd_ECHO(xbase::DbArea&, std::istringstream& iss) {
    std::string rest;
    std::getline(iss, rest);
    if (!rest.empty() && rest.front() == ' ')
        rest.erase(0, 1);
    std::cout << rest << std::endl;
}
