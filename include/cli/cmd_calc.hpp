// include/cli/cmd_calc.hpp
#pragma once
#include <sstream>

namespace xbase { class DbArea; }

// Canonical CALC command signature used across the CLI.
void cmd_CALC(xbase::DbArea& area, std::istringstream& args);



