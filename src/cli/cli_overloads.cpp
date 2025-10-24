// src/cli/cli_overloads.cpp  (overlay replacement)
// Minimal current-area glue only; intentionally no cmd_* overloads here.
#include <sstream>

namespace xbase { class DbArea; }

namespace cli_shim {
    // Thread-local handle to the current DbArea used by legacy (no-DbArea) call paths.
    thread_local xbase::DbArea* g_current_area = nullptr;
}

extern "C" {
    xbase::DbArea* cli_current_area_get() { return cli_shim::g_current_area; }
    void cli_current_area_set(xbase::DbArea* p) { cli_shim::g_current_area = p; }
}

// Note: This overlay intentionally removes any definitions like
//   void cmd_LIST(xbase::DbArea&, std::istringstream&);
//   void cmd_STATUS(xbase::DbArea&, std::istringstream&);
// etc.
// Those were duplicating the real implementations provided in cmd_*.cpp and
// caused LNK2005 duplicate symbol errors. The shell will call the DbArea&
// variants directly from their original translation units.
