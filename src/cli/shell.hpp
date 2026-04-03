#ifndef DOTTALK_CLI_SHELL_HPP
#define DOTTALK_CLI_SHELL_HPP

#include <string>
#include "xbase.hpp"

// Shell entry
int run_shell();

// ------------------------------------------------------------
// IF / ELSE / ENDIF control (implemented in shell.cpp / control utils)
// ------------------------------------------------------------
bool shell_if_can_eval();
bool shell_if_is_suppressed();

void shell_if_enter(bool condition, bool evaluated);
void shell_if_else();
void shell_if_exit();

// ------------------------------------------------------------
// Boolean evaluation (shared with WHILE / UNTIL / IF)
// Returns true if evaluation succeeded.
// On success, *result receives the condition truth value.
// On failure, returns false and optionally fills err.
// ------------------------------------------------------------
bool shell_eval_bool_expr(xbase::DbArea& area,
                          const std::string& expr,
                          bool* result,
                          std::string* err = nullptr);

#endif // DOTTALK_CLI_SHELL_HPP