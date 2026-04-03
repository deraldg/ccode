// include/cli/cmd_aggs.hpp
#pragma once
#include <sstream>

namespace xbase { class DbArea; }

// Dispatcher form:
//   AGGS SUM <value_expr> [FOR <pred>] [WHERE <pred>] [DELETED|NOT DELETED|!DELETED]
//   AGGS AVG ...
//   AGGS MIN ...
//   AGGS MAX ...
void cmd_AGGS(xbase::DbArea& area, std::istringstream& args);

// Direct forms (if you want to register these names separately)
void cmd_SUM(xbase::DbArea& area, std::istringstream& args);
void cmd_AVG(xbase::DbArea& area, std::istringstream& args);
void cmd_MIN(xbase::DbArea& area, std::istringstream& args);
void cmd_MAX(xbase::DbArea& area, std::istringstream& args);
