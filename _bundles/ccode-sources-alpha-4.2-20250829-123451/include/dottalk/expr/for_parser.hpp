#pragma once
#include <sstream>
#include <string>

namespace dottalk { namespace expr {

// Extracts a trailing 'FOR <expr>' from the current position of `iss` if present.
// - Returns true and sets `out` when found.
// - Returns false and leaves `iss` at its original position when not found.
bool extract_for_clause(std::istringstream& iss, std::string& out);

}} // namespace
