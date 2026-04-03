// src/xbase/dbarea_introspection.cpp
#include "xbase.hpp"
#include <cstdint>
#include <string>

// NOTE:
// This unit previously provided ad-hoc implementations of
//   - int DbArea::recordCount() const
//   - int DbArea::recordLength() const
//   - std::string DbArea::filename() const
// These conflicted with the standardized API:
//   - recCount()  : canonical count accessor (inline in xbase.hpp)
//   - recLength() : canonical record-length accessor (inline) and
//                   legacy recordLength() defined once in dbarea.cpp
//   - filename()  : defined once in dbarea.cpp
// To maintain the agreed naming convention (rec* prefix) and avoid duplicate
// definitions, those implementations were removed from this file.
// If future introspection helpers are added, prefer free functions that
// accept a DbArea& rather than redefining members.

namespace {

// Guard that restores the current record on destruction.
// (Kept here for potential future probing helpers.)
struct RecGuard {
    xbase::DbArea& a;
    int32_t saved;
    explicit RecGuard(xbase::DbArea& area) : a(area), saved(area.recno()) {}
    ~RecGuard() { if (saved > 0 && a.recno() != saved) a.gotoRec(saved); }
};

} // namespace

namespace xbase {
// Intentionally empty: see note above.
} // namespace xbase



