#pragma once

#include <sstream>
#include "xbase.hpp"

// TUPTALK / TT ? tuple-based normalization test harness.
// Syntax:
//
//   TUPTALK
//   TUPTALK RESET
//   TUPTALK ADD <type> <len> [<dec>] <raw...>
//   TUPTALK LIST
//   TUPTALK NORMALIZE
//   TUPTALK DUMP
//
// Notes:
//   - <type> is one of C, N, D, L (case-insensitive).
//   - For N, a decimal count is required: N <len> <dec> <raw...>
//   - For C/D/L, <dec> is omitted: <type> <len> <raw...>
//   - Raw is stored exactly as typed (including quotes).
//
// Aliases:
//   - TT is wired in shell.cpp to call cmd_TUPTALK.

// Simple CLI hook for the TUPTALK / TT tuple test harness.
// Note: DbArea is currently unused; kept for consistency with other commands.
void cmd_TUPTALK(xbase::DbArea& area, std::istringstream& iss);


