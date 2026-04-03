// src/cli/logical_nav.cpp
//
// Logical navigation helpers:
//   raw order stream
//     + active order direction
//     + SET FILTER visibility
//   => logical visible record stream
//
// This layer is intentionally above order_iterator/order_nav.
// It does not change backend order semantics; it only filters them
// into the user-visible logical view.

#include "cli/logical_nav.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "cli/order_iterator.hpp"
#include "filters/filter_registry.hpp"

namespace cli::logical_nav {
namespace {

struct CursorRestore {
    xbase::DbArea& area;
    int32_t saved{0};
    bool had_open{false};

    explicit CursorRestore(xbase::DbArea& a)
        : area(a), saved(a.isOpen() ? a.recno() : 0), had_open(a.isOpen()) {}

    ~CursorRestore() {
        if (!had_open) return;
        if (saved > 0) {
            (void)area.gotoRec(saved);
            (void)area.readCurrent();
        }
    }
};

static bool collect_display_recnos(xbase::DbArea& area,
                                   std::vector<uint64_t>& out_recnos)
{
    out_recnos.clear();

    cli::OrderIterSpec spec{};
    std::string err;
    if (!cli::order_collect_recnos_asc(area, out_recnos, &spec, &err)) {
        return false;
    }

    if (!spec.ascending) {
        std::reverse(out_recnos.begin(), out_recnos.end());
    }

    return true;
}

static int find_index_of_recno(const std::vector<uint64_t>& recnos, int32_t recno)
{
    for (size_t i = 0; i < recnos.size(); ++i) {
        if (static_cast<int32_t>(recnos[i]) == recno) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

bool is_visible(xbase::DbArea& area, int32_t recno)
{
    if (!area.isOpen() || recno <= 0 || recno > area.recCount()) {
        return false;
    }

    CursorRestore restore(area);

    if (!area.gotoRec(recno) || !area.readCurrent()) {
        return false;
    }

    // Canonical logical visibility gate already used by LIST/COUNT/SMARTLIST/LOCATE.
    return filter::visible(&area, std::shared_ptr<dottalk::expr::Expr>{});
}

int32_t first_recno(xbase::DbArea& area)
{
    if (!area.isOpen()) return 0;

    std::vector<uint64_t> recnos;
    if (!collect_display_recnos(area, recnos)) return 0;

    for (uint64_t rn64 : recnos) {
        const int32_t rn = static_cast<int32_t>(rn64);
        if (is_visible(area, rn)) return rn;
    }
    return 0;
}

int32_t last_recno(xbase::DbArea& area)
{
    if (!area.isOpen()) return 0;

    std::vector<uint64_t> recnos;
    if (!collect_display_recnos(area, recnos)) return 0;

    for (auto it = recnos.rbegin(); it != recnos.rend(); ++it) {
        const int32_t rn = static_cast<int32_t>(*it);
        if (is_visible(area, rn)) return rn;
    }
    return 0;
}

int32_t next_recno(xbase::DbArea& area, int32_t from_recno)
{
    if (!area.isOpen()) return 0;
    if (from_recno == 0) return first_recno(area);

    std::vector<uint64_t> recnos;
    if (!collect_display_recnos(area, recnos)) return 0;

    const int idx = find_index_of_recno(recnos, from_recno);
    if (idx < 0) return 0;

    for (size_t i = static_cast<size_t>(idx + 1); i < recnos.size(); ++i) {
        const int32_t rn = static_cast<int32_t>(recnos[i]);
        if (is_visible(area, rn)) return rn;
    }
    return 0;
}

int32_t prev_recno(xbase::DbArea& area, int32_t from_recno)
{
    if (!area.isOpen()) return 0;
    if (from_recno == 0) return last_recno(area);

    std::vector<uint64_t> recnos;
    if (!collect_display_recnos(area, recnos)) return 0;

    const int idx = find_index_of_recno(recnos, from_recno);
    if (idx < 0) return 0;

    for (int i = idx - 1; i >= 0; --i) {
        const int32_t rn = static_cast<int32_t>(recnos[static_cast<size_t>(i)]);
        if (is_visible(area, rn)) return rn;
    }
    return 0;
}

} // namespace cli::logical_nav
