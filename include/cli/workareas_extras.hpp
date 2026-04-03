#pragma once
#include <string>
#include <optional>

namespace workareas_extras {

// Count of available work areas (0..count-1 are valid).
int max_areas();

// Is an area currently ?open? (i.e., has a DBF assigned)?
bool is_open(int area);

// Full path (or filename) for area; empty if unknown/closed.
std::string path_of(int area);

// Try to resolve a token like "students" or "students.dbf" to an area
// by comparing the stem case-insensitively across *all* areas including 0.
int try_resolve_by_name(const std::string& token);

// Select an area by number. Returns true on success.
bool select(int area);

// Select by token "students" (or "students.dbf"). Returns true on success.
bool select_by_token(const std::string& token);

} // namespace workareas_extras



