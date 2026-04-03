// src/cli/cmd_delete.cpp — DELETE with implicit filesystem record locking
//
// Incremental indexing behavior:
// - Direct-write mode: if an active CDX/LMDB tag is field-backed, remove the
//   current record's active-tag key from the index after delete succeeds.
// - Buffered/table mode: unchanged here; COMMIT remains responsible for rebuild.
//
// SET FILTER behavior:
// - DELETE honors active SET FILTER in ALL / REST / NEXT / FOR scans.
// - No-arg DELETE (current record) remains explicit on the current row.
//
// Traversal behavior:
// - DELETE snapshots target recnos first, then deletes by recno.
// - This avoids mutating an active index while simultaneously iterating it.
// - ALL / FOR use ordered snapshot traversal when an order is active.
// - REST / NEXT preserve current navigation semantics through the shared selector.

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "textio.hpp"
#include "xbase.hpp"
#include "xbase_locks.hpp"
#include "command_registry.hpp"
#include "cli/table_state.hpp"
#include "cli/scan_selector.hpp"
#include "cli/expr/normalize_where.hpp"
#include "xindex/index_manager.hpp"

// Provided by the interactive shell.
extern "C" xbase::XBaseEngine* shell_engine(void);

using namespace std;

namespace {

struct CursorRestore {
    xbase::DbArea* area{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& a) : area(&a) {
        saved = a.recno();
        active = (saved >= 1 && saved <= a.recCount());
    }

    ~CursorRestore() {
        if (!active || !area) return;
        try {
            (void)area->gotoRec(saved);
            (void)area->readCurrent();
        } catch (...) {
            // best effort
        }
    }
};

static int resolve_current_index(xbase::DbArea& A) {
    xbase::XBaseEngine* eng = shell_engine();
    if (!eng) return -1;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (&eng->area(i) == &A) return i;
    }
    return -1;
}

static void mark_all_fields_stale_best_effort(xbase::DbArea& A, int area0) {
    if (area0 < 0) return;
    try {
        const int n = A.fieldCount();
        for (int field1 = 1; field1 <= n; ++field1) {
            dottalk::table::mark_stale_field(area0, field1);
        }
    } catch (...) {
        // best-effort only
    }
}

static bool parse_for_clause(std::istringstream& iss,
                             std::string& fld, std::string& op, std::string& val)
{
    std::streampos pos = iss.tellg();
    std::string kw;
    if (!(iss >> kw)) { iss.clear(); iss.seekg(pos); return false; }
    if (textio::up(kw) != "FOR") { iss.clear(); iss.seekg(pos); return false; }
    if (!(iss >> fld)) { iss.clear(); iss.seekg(pos); return false; }
    if (!(iss >> op )) { iss.clear(); iss.seekg(pos); return false; }

    std::string rest;
    std::getline(iss, rest);
    val = textio::unquote(textio::trim(rest));

    return !fld.empty() && !op.empty();
}

static bool delete_current_with_lock(xbase::DbArea& area)
{
    const auto rn = static_cast<uint32_t>(area.recno());
    if (rn == 0) return false;

    std::string err;
    if (!xbase::locks::try_lock_record(area, rn, &err)) return false;

    bool ok = false;

    try {
        if (!area.readCurrent()) {
            ok = false;
        } else if (area.isDeleted()) {
            // Already deleted.
            ok = true;
        } else {
            // Capture active-tag key before deleting, while the row is still live.
            xindex::Key active_key;
            bool have_active_key = false;

            try {
                auto& im = area.indexManager();
                active_key = im.buildActiveTagBaseKeyFromCurrentRecord();
                have_active_key = !active_key.empty();
            } catch (...) {
                have_active_key = false;
            }

            ok = area.deleteCurrent();

            if (ok && have_active_key) {
                try {
                    area.indexManager().on_delete(active_key, static_cast<xindex::RecNo>(rn));
                } catch (...) {
                    // Best effort only. Preserve DELETE success even if index update fails.
                }
            }
        }
    } catch (...) {
        ok = false;
    }

    xbase::locks::unlock_record(area, rn);

    if (ok) {
        const int area0 = resolve_current_index(area);
        mark_all_fields_stale_best_effort(area, area0);
    }

    return ok;
}

static int32_t delete_targets_by_recno(xbase::DbArea& area,
                                       const std::vector<uint64_t>& targets)
{
    int32_t cnt = 0;
    for (uint64_t rn : targets) {
        if (!area.gotoRec((int32_t)rn)) continue;
        if (!area.readCurrent()) continue;
        if (delete_current_with_lock(area)) ++cnt;
    }
    return cnt;
}

} // namespace

void cmd_DELETE(xbase::DbArea& area, std::istringstream& iss)
{
    if (!area.isOpen()) {
        std::cout << "No table is open. Use USE <file> first.\n";
        return;
    }

    CursorRestore restore(area);

    // No-arg (current record)
    std::string tok;
    std::streampos savepos = iss.tellg();
    if (!(iss >> tok)) {
        if (!area.readCurrent()) {
            std::cout << "0 deleted\n";
            return;
        }
        std::cout << (delete_current_with_lock(area) ? 1 : 0) << " deleted\n";
        return;
    }
    iss.clear();
    iss.seekg(savepos);

    // Mode: FOR ...
    std::string fld, op, val;
    if (parse_for_clause(iss, fld, op, val)) {
        cli::scan::SelectionSpec spec{};
        spec.scan_mode = cli::scan::ScanMode::All;
        spec.use_expr = true;

        std::string expr = fld + " " + op + " " + val;
        spec.expr = normalize_unquoted_rhs_literals(area, expr);

        const auto sel = cli::scan::collect_selected_recnos(area, spec);
        const int32_t cnt = delete_targets_by_recno(area, sel.recnos);
        std::cout << cnt << " deleted\n";
        return;
    }

    // Modes: ALL | REST | NEXT n
    std::string mode;
    iss >> mode;
    const auto UM = textio::up(mode);

    if (UM == "ALL") {
        cli::scan::SelectionSpec spec{};
        spec.scan_mode = cli::scan::ScanMode::All;

        const auto sel = cli::scan::collect_selected_recnos(area, spec);
        const int32_t cnt = delete_targets_by_recno(area, sel.recnos);
        std::cout << cnt << " deleted\n";
        return;
    }

    if (UM == "REST") {
        cli::scan::SelectionSpec spec{};
        spec.scan_mode = cli::scan::ScanMode::Rest;

        const auto sel = cli::scan::collect_selected_recnos(area, spec);
        const int32_t cnt = delete_targets_by_recno(area, sel.recnos);
        std::cout << cnt << " deleted\n";
        return;
    }

    if (UM == "NEXT") {
        int n = 0;
        if (!(iss >> n) || n <= 0) {
            std::cout << "Usage: DELETE NEXT <n>\n";
            return;
        }

        cli::scan::SelectionSpec spec{};
        spec.scan_mode = cli::scan::ScanMode::NextN;
        spec.next_n = n;

        const auto sel = cli::scan::collect_selected_recnos(area, spec);
        const int32_t cnt = delete_targets_by_recno(area, sel.recnos);
        std::cout << cnt << " deleted\n";
        return;
    }

    std::cout << "Usage: DELETE [ALL | REST | NEXT <n> | FOR <field> <op> <value>]  (no args => delete current record)\n";
}

static bool s_registered = [](){
    dli::registry().add("DELETE", &cmd_DELETE);
    return true;
}();