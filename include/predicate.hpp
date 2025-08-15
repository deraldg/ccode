#pragma once
#include <string>
#include "xbase.hpp"

// Simple wrapper that parses "FIELD OP VALUE" and calls predicates::eval
bool eval_cond(const xbase::DbArea& A, const std::string& expr);
