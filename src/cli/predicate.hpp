#pragma once
#include <string>
#include "xbase.hpp"

// Simple wrapper that parses "FIELD OP VALUE" and calls predicates::eval
bool eval_cond(const xbase::DbArea& A, const std::string& expr);

// global-scope forward decl (definition lives in src/cli/predicates.cpp)
int field_index_ci(const xbase::DbArea& a, const std::string& name);



