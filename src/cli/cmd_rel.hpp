#pragma once
#include <sstream>

namespace xbase { class DbArea; }

// Wrapper for the REL family (subcommand dispatcher).
void cmd_REL(xbase::DbArea& area, std::istringstream& in);
