#pragma once
#include <sstream>
namespace xbase { class DbArea; }

// Match whatever your registry expects. Most of your cmds are void, so:
void cmd_TVISION(xbase::DbArea& area, std::istringstream& args);
