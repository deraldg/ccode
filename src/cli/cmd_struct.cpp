// src/cli/cmd_struct.cpp
#include <sstream>
#include "xbase.hpp"

// Reuse the FIELDS implementation
void cmd_FIELDS(xbase::DbArea&, std::istringstream&); // already defined

void cmd_STRUCT(xbase::DbArea& a, std::istringstream& iss) {
    cmd_FIELDS(a, iss);
}
