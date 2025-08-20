#include "cmd_version.hpp"
#include <iostream>

#ifndef DOTTALKPP_VERSION
#define DOTTALKPP_VERSION "alpha-v3.1"
#endif

#ifndef DTX_VERSION_STRING
#define DTX_VERSION_STRING "DotTalk++ alpha v4.0"
#endif
#ifndef DTX_BUILD_STAMP
#define DTX_BUILD_STAMP __DATE__ " " __TIME__
#endif

void cmd_VERSION(xbase::DbArea& area, std::istringstream& args) {
    (void)area; (void)args;
//  std::cout << "dottalk++ " << DOTTALKPP_VERSION
//      << "  (" << __DATE__ << " " << __TIME__ << ")\n";
// inside cmd_VERSION(...)
    std::cout << DTX_VERSION_STRING << "  (" << DTX_BUILD_STAMP << ")\n";

}

