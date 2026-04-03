#pragma once

#include <string>
#include <unordered_map>
#include "xbase.hpp"  // xbase::DbArea

namespace dottalk {

// Global shell macro variables (FoxPro-ish: SET VAR name = <text>, &name)
extern std::unordered_map<std::string, std::string> g_shell_vars;

// Check if char can start an identifier
bool is_ident_start(char c);

// Check if char can continue an identifier
bool is_ident_char(char c);

// Expand &name macros outside quotes
bool expand_macros_outside_quotes(const std::string& in,
                                  std::string& out,
                                  std::string& err_name);

// Quote string for shell storage (FoxPro-ish doubling of quotes)
std::string quote_dottalk_string(const std::string& raw);

// Result from try_handle_var_command
struct VarCmdResult {
    bool handled = false;
    bool ok = true;
};

// Handle SET/SHOW/CLEAR VAR commands
VarCmdResult try_handle_var_command(xbase::DbArea& area, const std::string& preparedLine);

} // namespace dottalk