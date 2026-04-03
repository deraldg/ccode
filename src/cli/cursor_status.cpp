#include "cursor_status.hpp"
#include "xbase.hpp"

#include <sstream>
#include <string>
#include <type_traits>

namespace xbase::cursor_status {
namespace {

template <class T>
concept HasRecno = requires(T& t) {
    { t.recno() } -> std::convertible_to<int>;
};

template <class T>
concept HasLogicalRow = requires(T& t) {
    { t.logicalRow() } -> std::convertible_to<long long>;
};

template <class T>
concept HasHasOrder = requires(T& t) {
    { t.hasOrder() } -> std::convertible_to<bool>;
};

template <class T>
concept HasHasFilter = requires(T& t) {
    { t.hasFilter() } -> std::convertible_to<bool>;
};

template <class T>
concept HasDeletedOn = requires(T& t) {
    { t.deletedOn() } -> std::convertible_to<bool>;
};

template <class T>
int probe_physical_recno(T& area) noexcept {
    if constexpr (HasRecno<T>) {
        return static_cast<int>(area.recno());
    } else {
        return 0;
    }
}

template <class T>
bool probe_has_order(T& area) noexcept {
    if constexpr (HasHasOrder<T>) {
        return static_cast<bool>(area.hasOrder());
    } else {
        return false;
    }
}

template <class T>
bool probe_has_filter(T& area) noexcept {
    if constexpr (HasHasFilter<T>) {
        return static_cast<bool>(area.hasFilter());
    } else {
        return false;
    }
}

template <class T>
bool probe_deleted_on(T& area) noexcept {
    if constexpr (HasDeletedOn<T>) {
        return static_cast<bool>(area.deletedOn());
    } else {
        return false;
    }
}

template <class T>
long long probe_logical_row(T& area,
                            bool has_order,
                            bool has_filter,
                            int physical_recno) noexcept {
    if constexpr (HasLogicalRow<T>) {
        return static_cast<long long>(area.logicalRow());
    } else {
        if (!has_order && !has_filter) {
            return static_cast<long long>(physical_recno);
        }
        return -1;
    }
}

} // anonymous namespace

CursorSnapshot build_snapshot(DbArea& area,
                              const char* reason,
                              int area_first_hint,
                              int area_last_hint) noexcept
{
    CursorSnapshot s{};

    s.area_index     = -1; // caller can override if desired
    s.area_first     = area_first_hint;
    s.area_last      = area_last_hint;
    s.physical_recno = probe_physical_recno(area);
    s.has_order      = probe_has_order(area);
    s.has_filter     = probe_has_filter(area);
    s.deleted_on     = probe_deleted_on(area);
    s.logical_row    = probe_logical_row(area, s.has_order, s.has_filter, s.physical_recno);
    s.reason         = reason ? reason : "";

    return s;
}

std::string format_cursor_line(const CursorSnapshot& s)
{
    std::ostringstream out;

    out << "Cursor: Area ";

    if (s.area_index >= 0) out << s.area_index;
    else                   out << "?";

    out << " of " << s.area_first << " ... " << s.area_last
        << "  Physical Recno " << s.physical_recno
        << ", Logical Row ";

    if (s.logical_row >= 0) out << s.logical_row;
    else                    out << "?";

    return out.str();
}

} // namespace xbase::cursor_status