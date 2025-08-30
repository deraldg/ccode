// src/cli/cmd_zap.cpp
#include "xbase.hpp"
#include <sstream>
#include <cstdio>  // fprintf

// Must be global, exact signature:
void cmd_ZAP(xbase::DbArea& area, std::istringstream& args) {
    (void)area; (void)args;
    std::fprintf(stderr, "ZAP is disabled in this build.\n");
}
