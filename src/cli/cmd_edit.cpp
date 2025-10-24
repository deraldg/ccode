// src/cli/cmd_edit.cpp
#include "xbase.hpp"
#include <sstream>

// forward to REPLACE (keep one source of truth)
void cmd_REPLACE(xbase::DbArea&, std::istringstream&);

void cmd_EDIT(xbase::DbArea& a, std::istringstream& iss) {
    cmd_REPLACE(a, iss);
}
