// src/dli/recno_shim.cpp — provide dli::dli_current_recno expected by browse_edit.cpp
#include "xbase.hpp"

namespace dli {
// MSVC expects a 64-bit return (__int64) for this symbol.
// Provide a thin wrapper around DbArea::recno().
long long dli_current_recno(xbase::DbArea& db) {
    return static_cast<long long>(db.recno());
}
} // namespace dli
