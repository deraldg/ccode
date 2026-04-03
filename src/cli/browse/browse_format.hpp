#pragma once
#include <string>

namespace xbase { class DbArea; }

namespace dottalk::browse::format {

// Pretty printer with fixed-width columns by field type.
std::string tuple_pretty(::xbase::DbArea& db);

// Raw concatenation (legacy behavior).
std::string tuple_raw(::xbase::DbArea& db);

} // namespace dottalk::browse::format
