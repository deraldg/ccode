#pragma once
// include/cli/expr/fn_date.hpp
//
// DotTalk++ date/time builtins (FoxPro-ish).
//
// Return formats (DBF-friendly numeric strings):
//   DATE/TODAY     -> YYYYMMDD
//   TIME           -> HHMMSS
//   NOW/DATETIME   -> YYYYMMDDHHMMSS
//
// Also provides:
//   CTOD(x)        -> YYYYMMDD  (accepts YYYY-MM-DD, YYYY/MM/DD, YYYYMMDD, etc.)
//   DTOC(X)
//   DATEADD(d,n)   -> YYYYMMDD  (adds n days)
//   DATEDIFF(a,b)  -> days (a - b)
//
// Uses the same BuiltinFnSpec contract as fn_string.

#include "cli/expr/fn_string.hpp" // BuiltinFnSpec / BuiltinFnEval

#include <cstddef>

namespace dottalk::expr {

const BuiltinFnSpec* date_fn_specs();
std::size_t          date_fn_specs_count();

} // namespace dottalk::expr
