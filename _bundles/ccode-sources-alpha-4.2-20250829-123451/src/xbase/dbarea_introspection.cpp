// src/xbase/dbarea_introspection.cpp
#include "xbase.hpp"
#include <cstdint>
#include <string>

namespace {

// Guard that restores the current record on destruction.
struct RecGuard {
    xbase::DbArea& a;
    int32_t saved;
    explicit RecGuard(xbase::DbArea& area) : a(area), saved(area.recno()) {}
    ~RecGuard() { if (saved > 0 && a.recno() != saved) a.gotoRec(saved); }
};

} // namespace

namespace xbase {

int DbArea::recordCount() const {
    // We keep this logically-const: we probe using gotoRec and restore.
    auto& self = *const_cast<DbArea*>(this);
    RecGuard guard(self);

    // Exponential search to find an upper bound
    int32_t lo = 0;
    int32_t hi = 1;
    while (self.gotoRec(hi)) {
        lo = hi;
        if (hi >= (1 << 30)) break;   // hard safety cap
        hi <<= 1;
    }

    // Binary search in (lo, hi)
    int32_t best = lo;
    int32_t L = lo + 1, R = hi - 1;
    while (L <= R) {
        const int32_t mid = L + (R - L) / 2;
        if (self.gotoRec(mid)) { best = mid; L = mid + 1; }
        else                   { R = mid - 1; }
    }
    return static_cast<int>(best);
}

int DbArea::recordLength() const {
    // TODO: wire to real header once exposed
    return -1;
}

std::string DbArea::filename() const {
    // TODO: wire to real filename/path once exposed
    return std::string{};
}

} // namespace xbase
