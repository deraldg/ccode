// src/cli/cursor_guard.hpp
#pragma once

#include <cstdint>
#include "xbase.hpp"

/*
CursorGuard: make a command cursor-preserving.

Use in commands that must *not* change the user's current record (LIST/WORKSPACE/BROWSE pages, etc.)
even if they iterate by calling gotoRec/skip/top/bottom internally.
*/
namespace cli {

class CursorGuard {
public:
    explicit CursorGuard(xbase::DbArea& area) noexcept
        : _area(area), _open(area.isOpen()), _recno(area.recno()) {}

    CursorGuard(const CursorGuard&) = delete;
    CursorGuard& operator=(const CursorGuard&) = delete;

    ~CursorGuard() noexcept {
        if (!_open) return;
        if (_recno <= 0) return;
        try {
            _area.gotoRec(_recno);
            _area.readCurrent();
        } catch (...) {}
    }

private:
    xbase::DbArea& _area;
    bool _open{false};
    int32_t _recno{0};
};

} // namespace cli
