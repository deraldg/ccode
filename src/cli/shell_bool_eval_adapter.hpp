#pragma once

#include <string>
#include "xbase.hpp"

using ShellAstBoolEval = bool(*)(xbase::DbArea& area,
                                 const std::string& expr,
                                 bool* result,
                                 std::string* err);

// Optional injection point for tests / alternate predicate engines.
void shell_set_ast_boolean_evaluator(ShellAstBoolEval fn);

// Shared boolean evaluator for IF / WHILE / UNTIL style conditions.
// Returns true if evaluation succeeded. On success, *result receives the
// condition truth value. On failure, returns false and optionally fills err.
bool shell_eval_bool_expr(xbase::DbArea& area,
                          const std::string& expr,
                          bool* result,
                          std::string* err = nullptr);

// Register the loop bridge used by cmd_WHILE / cmd_UNTIL.
void shell_eval_register_for_loops();
