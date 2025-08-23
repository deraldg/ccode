#include "cmd_version.hpp"
#include <iostream>

#ifndef DOTTALKPP_VERSION
#define DOTTALKPP_VERSION "alpha-v3.1"
#endif

void cmd_VERSION(xbase::DbArea& area, std::istringstream& args) {
    (void)area; (void)args;
    std::cout << "dottalk++ " << DOTTALKPP_VERSION
              << "  (" << __DATE__ << " " << __TIME__ << ")\n";
    // cmd_version.cpp
    std::cout << "DotTalk++ build " << __DATE__ << " " << __TIME__ << "\n";

}
 