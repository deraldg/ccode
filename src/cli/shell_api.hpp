#pragma once
#include <string>

namespace xbase { class DbArea; }

// Process exactly one CLI line (same semantics as a line typed in the REPL).
// Returns true if the command was handled (including unknown/usage cases);
// returns false if the line requested shell exit (e.g., QUIT).
bool shell_dispatch_line(xbase::DbArea& a, const std::string& line);
