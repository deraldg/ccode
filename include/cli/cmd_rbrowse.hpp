// include/cli/cmd_rbrowse.hpp
#pragma once

#include <sstream>

namespace xbase { class DbArea; }

// ERSATZ command (internal RBROWSE relational browser)
void cmd_RBROWSE(xbase::DbArea& area, std::istringstream& iss);