#pragma once

#include <sstream>

namespace xbase { class DbArea; }

// ABOUT
// Prints project identity / history / heritage information.
void cmd_ABOUT(xbase::DbArea& area, std::istringstream& iss);