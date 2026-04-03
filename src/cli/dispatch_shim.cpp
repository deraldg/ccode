// src/cli/dispatch_shim.cpp  (overlay addition)
// Provides the missing "dispatch_command(const std::string&)" used by reader.cpp
#include <string>
#include "xbase.hpp"

extern "C" xbase::DbArea* cli_current_area_get();

// Forward-declare the modern dispatcher (implemented in shell_api.cpp).
bool shell_dispatch_line(xbase::DbArea& area, const std::string& line);

void dispatch_command(const std::string& line) {
    if (auto* a = cli_current_area_get()) {
        (void)shell_dispatch_line(*a, line);
    }
    // If no current area is set, we silently do nothing. This preserves
    // compatibility with older reader paths without risking a null deref.
}



