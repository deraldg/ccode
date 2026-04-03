
// src/cli/predicate_chain.hpp
#pragma once
#include <string>
#include "xbase.hpp"

// Try to evaluate a SMARTLIST-style boolean chain in <expr> against the current record.
// Supported grammar (case-insensitive):
//   <cond>        := <field> <relop> <value>
//   <relop>       := = | == | <> | != | > | < | >= | <=
//   <value>       := quoted-string | bare-token
//   <chain>       := <cond> ( (AND|OR) <cond> )*
// Strings with spaces must be quoted. No parentheses in this minimal chain parser.
//
// Returns true if the expression was recognized as a chain (handled).
// When handled == true, out_result contains the boolean result.
// When handled == false, caller should fall back to the full expression engine.
bool try_eval_predicate_chain(const xbase::DbArea& area,
                              const std::string& expr,
                              bool& out_result,
                              std::string* error_msg = nullptr);



