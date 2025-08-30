#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include "xbase.hpp"

namespace predicates {

int field_index_ci(const xbase::DbArea& a, const std::string& name);
bool eval(const xbase::DbArea& a,
          const std::string& fld,
          const std::string& op,
          const std::string& val);

} // namespace predicates
