// src/cli/cmd_dbarea.hpp
#pragma once

#include <sstream>

namespace xbase { class DbArea; }

// DBAREA
//  - DBAREA                 (current slot)
//  - DBAREA <n>             (inspect slot n)
//  - DBAREA ALL             (inspect all open slots)
//  - DBAREA REL|LINKS       (append relation context for current parent)
//  - DBAREA <n> REL|LINKS   (REL/LINKS only meaningful for current slot; otherwise prints a note)
void cmd_DBAREA(xbase::DbArea& A, std::istringstream& S);
