#pragma once

#include <sstream>

namespace xbase { class DbArea; }

// WAMREPORT
// Dev-only bridge verification report for WorkAreaManager <-> XBaseEngine.
// Usage:
//   WAMREPORT
void cmd_WAMREPORT(xbase::DbArea& A, std::istringstream& S);
