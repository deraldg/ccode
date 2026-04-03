#pragma once
#include <sstream>
namespace xbase { class DbArea; }

// Match the command registry's expected signature
void cmd_PALETTE(xbase::DbArea& area, std::istringstream& args);


