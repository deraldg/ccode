// src/cli/cmd_area.cpp — AREA shows current-area details
// Thin wrapper that reuses STATUS printing so labels (e.g., "Active index")
// stay consistent across commands.

#include <sstream>
#include "xbase.hpp"

// STATUS is our canonical printer for workspace/order info.
extern void cmd_STATUS(xbase::DbArea&, std::istringstream&);

void cmd_AREA(xbase::DbArea& A, std::istringstream& iss) {
    // We simply delegate to STATUS (no args). This keeps one code path
    // for "Index file" / "Active index" labels and avoids drift.
    (void)iss; // unused
    std::istringstream none;
    cmd_STATUS(A, none);
}
