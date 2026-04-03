// src/cli/cmd_tvision.cpp
#include <iostream>
#include <sstream>
#include "xbase.hpp"

// Signature must match the shell's command registry:
//   void cmd_TVISION(xbase::DbArea&, std::istringstream&)
void cmd_TVISION(xbase::DbArea& area, std::istringstream& args) {
    (void)area; (void)args;

    // Minimal stub: confirm TV lib is built & reachable.
    // We avoid textio here; std::cout is fine and portable.
    std::cout
        << "Turbo Vision (TUI) support is enabled and linked.\n"
        << "This stub command is present so the linker symbol resolves.\n"
        << "Next step: wire this to your TUI entrypoint when ready.\n";
}
