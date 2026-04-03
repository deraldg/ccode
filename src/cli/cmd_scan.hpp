#pragma once
// src/cli/cmd_scan.hpp
//
// SCAN subsystem for DotTalk++
//
// Public command surface:
//   SCAN [FOR <expr>]
//   __SCAN_BUFFER <raw line>   // internal helper used by shell while buffering
//   ENDSCAN
//
// Behavioral contract:
//   - SCAN begins a buffered command block.
//   - __SCAN_BUFFER appends one raw command line into the active SCAN block.
//   - ENDSCAN executes the buffered lines once for each matching record.
//   - If an active order exists and ordered traversal is available, ENDSCAN
//     should traverse in current order.
//   - Otherwise traversal falls back to physical DBF order.
//   - Nested SCAN during ENDSCAN execution is not allowed.

#include <sstream>

namespace xbase { class DbArea; }

// SCAN subsystem commands
void cmd_SCAN        (xbase::DbArea& A, std::istringstream& S);
void cmd_SCAN_BUFFER (xbase::DbArea& A, std::istringstream& S); // internal helper used by shell
void cmd_ENDSCAN     (xbase::DbArea& A, std::istringstream& S);