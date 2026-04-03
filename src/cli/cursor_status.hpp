#pragma once

#include <string>

namespace xbase {
class DbArea;
}

namespace xbase::cursor_status {

struct CursorSnapshot {
    int area_index      = -1;
    int area_first      = 0;
    int area_last       = 255;

    int physical_recno  = 0;
    long long logical_row = -1;   // -1 = unknown / not available

    bool has_order      = false;
    bool has_filter     = false;
    bool deleted_on     = false;

    std::string reason;
};

CursorSnapshot build_snapshot(DbArea& area,
                              const char* reason = nullptr,
                              int area_first_hint = 0,
                              int area_last_hint = 255) noexcept;

std::string format_cursor_line(const CursorSnapshot& s);

} // namespace xbase::cursor_status