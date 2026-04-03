#pragma once
#include <sstream>

namespace xbase { class DbArea; }

// Unified HELP entrypoint (router).
// Registers as the handler for "HELP".
void cmd_HELP(xbase::DbArea& area, std::istringstream& args);



