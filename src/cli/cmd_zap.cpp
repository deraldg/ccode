#include "xbase.hpp"
#include <sstream>
#include <iostream>

// Keep symbol available for the linker even if not registered.
void cmd_ZAP(xbase::DbArea&, std::istringstream&) {
    std::cout << "ZAP is disabled in this build.\n";
}
