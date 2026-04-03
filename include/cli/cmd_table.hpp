#pragma once

#include <sstream>

namespace xbase { class DbArea; }

// TABLE buffering (planned). Phase 0: command stubs + help.
// Now includes table_buffer support (auto-init on TABLE ON, COMMIT/ROLLBACK stubs)
void cmd_TABLE(xbase::DbArea& A, std::istringstream& in);
void cmd_COMMIT(xbase::DbArea& A, std::istringstream& in);
void cmd_ROLLBACK(xbase::DbArea& A, std::istringstream& in);