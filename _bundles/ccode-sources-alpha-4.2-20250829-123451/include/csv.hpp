#pragma once
#include <string>
#include <vector>

namespace csv {

std::vector<std::string> split_line(const std::string& line);
std::string escape(const std::string& s);

} // namespace csv
