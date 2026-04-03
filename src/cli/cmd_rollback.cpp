// src/cli/cmd_rollback.cpp
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cli/table_state.hpp"

extern "C" xbase::XBaseEngine* shell_engine();

namespace {

static inline std::string up_copy(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

static int resolve_current_index(xbase::DbArea& A) {
    xbase::XBaseEngine* eng = shell_engine();
    if (!eng) return -1;

    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (&eng->area(i) == &A) return i;
    }
    return -1;
}

static void rollback_area(int area0, size_t& total_changes, int& areas_touched) {
    // We consider an area "touched" if it had buffered changes OR flags set.
    bool touched = false;

    auto& tb = dottalk::table::get_tb(area0);
    if (!tb.empty()) {
        total_changes += tb.changes.size();
        tb.clear();
        touched = true;
    }

    // If TABLE is enabled, clear state flags (even if buffer was empty).
    // Contract: rollback means clean/fresh.
    if (dottalk::table::is_enabled(area0)) {
        if (dottalk::table::is_dirty(area0) || dottalk::table::is_stale(area0)) touched = true;

        dottalk::table::set_dirty(area0, false);
        dottalk::table::set_stale(area0, false);   // should clear stale bitset internally if implemented that way
        dottalk::table::clear_stale_fields(area0); // explicit/defensive
    }

    if (touched) ++areas_touched;
}

} // namespace

void cmd_ROLLBACK(xbase::DbArea& A, std::istringstream& in) {
    std::string tok;
    if (in >> tok) {
        const std::string up = up_copy(tok);
        if (up == "ALL") {
            auto* eng = shell_engine();
            if (!eng) {
                std::cout << "ROLLBACK: engine unavailable.\n";
                return;
            }

            size_t total_changes = 0;
            int areas_touched = 0;

            for (int i = 0; i < xbase::MAX_AREA; ++i) {
                rollback_area(i, total_changes, areas_touched);
            }

            std::cout << "ROLLBACK ALL: discarded " << total_changes
                      << " change(s) across " << areas_touched << " area(s).\n";
            return;
        }

        std::cout << "Usage: ROLLBACK [ALL]\n";
        return;
    }

    const int area0 = resolve_current_index(A);
    if (area0 < 0) {
        std::cout << "ROLLBACK: cannot determine current area.\n";
        return;
    }

    size_t total_changes = 0;
    int areas_touched = 0;

    rollback_area(area0, total_changes, areas_touched);

    // For single-area rollback, keep the classic wording.
    std::cout << "ROLLBACK: discarded " << total_changes << " change(s).\n";
}
