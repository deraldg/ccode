// src/cli/cmd_commit.cpp
//
// COMMIT [ALL] [MANUAL|INTERACTIVE|AUTO]
//
// Contract:
// - TABLE ON buffers changes; no OS locking should occur during REPLACE/DELETE.
// - COMMIT applies buffered changes with locking at commit time.
// - COMMIT auto-reindexes after successful apply:
//     * INX/IDX     -> REINDEX
//     * CNX         -> REBUILD
//     * CDX/LMDB    -> BUILDLMDB
// - COMMIT ALL commits all open areas with buffered changes.
//
// Commit-time rebuild policy:
// - Default COMMIT flow auto-confirms BUILDLMDB so the shell does not pause.
// - MANUAL / INTERACTIVE preserves the old prompt behavior for testing.
// - Manual BUILDLMDB command remains unchanged.
//
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <cctype>
#include <cstdint>
#include <limits>

#include "xbase.hpp"
#include "xbase_locks.hpp"

#include "cli/settings.hpp"
#include "cli/table_state.hpp"
#include "cli/order_state.hpp"

// Index rebuild commands (used for auto-reindex).
void cmd_REINDEX(xbase::DbArea& A, std::istringstream& args);
void cmd_REBUILD(xbase::DbArea& A, std::istringstream& args);
void cmd_BUILDLMDB(xbase::DbArea& A, std::istringstream& args);

extern "C" xbase::XBaseEngine* shell_engine();

using cli::Settings;

namespace {

static int resolve_area0(xbase::DbArea& A) {
    if (auto* eng = shell_engine()) {
        for (int i = 0; i < xbase::MAX_AREA; ++i) {
            if (&eng->area(i) == &A) return i;
        }
    }
    return -1;
}

struct CursorRestore {
    xbase::DbArea* area = nullptr;
    int saved_recno = 0;
    bool active = false;

    explicit CursorRestore(xbase::DbArea& A) : area(&A) {
        try {
            saved_recno = A.recno();
            active = (saved_recno >= 1 && saved_recno <= A.recCount());
        } catch (...) {
            active = false;
        }
    }

    ~CursorRestore() {
        if (!active || !area) return;
        try {
            if (saved_recno >= 1 && saved_recno <= area->recCount()) {
                if (area->gotoRec(saved_recno)) {
                    (void)area->readCurrent();
                }
            }
        } catch (...) {
            // best-effort only
        }
    }

    CursorRestore(const CursorRestore&) = delete;
    CursorRestore& operator=(const CursorRestore&) = delete;
};

struct CinRedirectGuard {
    std::streambuf* old_buf = nullptr;
    std::istringstream fake_input;

    explicit CinRedirectGuard(const std::string& injected)
        : fake_input(injected)
    {
        old_buf = std::cin.rdbuf(fake_input.rdbuf());
    }

    ~CinRedirectGuard() {
        if (old_buf) {
            std::cin.rdbuf(old_buf);
        }
    }

    CinRedirectGuard(const CinRedirectGuard&) = delete;
    CinRedirectGuard& operator=(const CinRedirectGuard&) = delete;
};

struct Agg {
    int recno = 0;
    std::uint64_t flags = 0;
    // last-write-wins per field1
    std::unordered_map<int, std::string> field_values;
};

// Aggregate all ChangeEntry rows for the same recno into one view.
static Agg aggregate_for_recno(const dottalk::table::TableBuffer& tb, int recno) {
    Agg a;
    a.recno = recno;

    auto range = tb.changes.equal_range(recno);

    // For history mode, choose highest priority per field.
    std::unordered_map<int, int> best_prio;

    for (auto it = range.first; it != range.second; ++it) {
        const auto& e = it->second;
        a.flags |= e.dirty_flags;

        for (const auto& kv : e.new_values) {
            const int f1 = kv.first;
            const std::string& v = kv.second;

            if (!tb.history_enabled) {
                a.field_values[f1] = v;
            } else {
                const int pr = e.priority;
                auto bp = best_prio.find(f1);
                if (bp == best_prio.end() || pr >= bp->second) {
                    best_prio[f1] = pr;
                    a.field_values[f1] = v;
                }
            }
        }
    }

    return a;
}

static bool apply_one_recno(xbase::DbArea& A, const Agg& agg, bool talk) {
    const int rn = agg.recno;
    if (rn < 1 || rn > A.recCount()) return false;

    if (!A.gotoRec(rn)) return false;
    if (!A.readCurrent()) return false;

    // Lock at commit time (per-record). If you later add table locks, this is where it goes.
    std::string lock_err;
    if (!xbase::locks::try_lock_record(A, static_cast<std::uint32_t>(rn), &lock_err)) {
        if (talk) std::cout << "COMMIT: rec " << rn << " locked (" << lock_err << ")\n";
        return false;
    }

    bool ok = true;

    if (agg.flags & dottalk::table::CHANGE_UPDATE) {
        for (const auto& kv : agg.field_values) {
            const int f1 = kv.first;
            const std::string& v = kv.second;
            if (!A.set(f1, v)) {
                ok = false;
                break;
            }
        }
        if (ok) ok = A.writeCurrent();
    }

    if (ok && (agg.flags & dottalk::table::CHANGE_DELETE)) {
        ok = A.deleteCurrent();
    }

    xbase::locks::unlock_record(A, static_cast<std::uint32_t>(rn));
    return ok;
}

static bool auto_reindex_if_needed(xbase::DbArea& A,
                                   int area0,
                                   bool talk,
                                   bool interactive_rebuild)
{
    if (area0 < 0) return true;
    if (!dottalk::table::is_stale(area0)) return true;

    // Order-state contract:
    // - .cdx => CDX semantics, currently backed by BUILDLMDB / LMDB env
    // - .cnx => CNX rebuild path
    // - else => legacy INX/IDX path
    if (orderstate::hasOrder(A)) {
        if (orderstate::isCdx(A)) {
            if (talk) {
            std::cout << "COMMIT: rebuilding CDX/LMDB";
            const std::string tag = orderstate::activeTag(A);
            if (!tag.empty()) std::cout << " tag " << tag;
            if (interactive_rebuild) std::cout << " (interactive)";
            std::cout << "...\n";
        }

        if (interactive_rebuild) {
            std::istringstream args("MANUAL");
            cmd_BUILDLMDB(A, args);
        } else {
            std::istringstream args("AUTO QUIET");
            cmd_BUILDLMDB(A, args);
        }
        return true;
    }

        if (orderstate::isCnx(A)) {
            if (talk) std::cout << "COMMIT: rebuilding CNX...\n";
            std::istringstream args("");
            cmd_REBUILD(A, args);
            return true;
        }
    }

    if (talk) std::cout << "COMMIT: reindexing INX...\n";
    std::istringstream args("");
    cmd_REINDEX(A, args);
    return true;
}

static void commit_one_area(xbase::DbArea& A,
                            int area0,
                            bool talk,
                            bool interactive_rebuild)
{
    auto& tb = dottalk::table::get_tb(area0);

    if (tb.empty()) {
        if (talk) std::cout << "COMMIT: no changes in buffer.\n";
        return;
    }

    CursorRestore restore(A);

    // Iterate unique recnos in the multimap.
    int applied_ok = 0;
    int applied_fail = 0;

    for (auto it = tb.changes.begin(); it != tb.changes.end(); ) {
        const int recno = it->first;
        const auto range = tb.changes.equal_range(recno);

        const Agg agg = aggregate_for_recno(tb, recno);
        const bool ok = apply_one_recno(A, agg, talk);

        if (ok) {
            ++applied_ok;
            it = tb.changes.erase(range.first, range.second);
        } else {
            ++applied_fail;
            // Keep the entries so the user can retry COMMIT.
            it = range.second;
        }
    }

    if (applied_fail == 0) {
        // Mark not-dirty before calling rebuild commands that may refuse dirty TABLE state.
        dottalk::table::set_dirty(area0, false);

        (void)auto_reindex_if_needed(A, area0, talk, interactive_rebuild);

        // Clean/fresh.
        tb.clear();
        dottalk::table::set_dirty(area0, false);
        dottalk::table::set_stale(area0, false);
        dottalk::table::clear_stale_fields(area0);

        if (talk) std::cout << "COMMIT: done. (" << applied_ok << " recs)\n";
    } else {
        // Keep dirty/stale; buffer still contains remaining failed recnos.
        dottalk::table::set_dirty(area0, true);
        if (talk) {
            std::cout << "COMMIT: partial. OK=" << applied_ok
                      << " FAIL=" << applied_fail << " (remaining buffered)\n";
        } else {
            std::cout << "COMMIT: partial. OK=" << applied_ok
                      << " FAIL=" << applied_fail << ".\n";
        }
    }
}

} // namespace

void cmd_COMMIT(xbase::DbArea& A, std::istringstream& in) {
    auto* eng = shell_engine();
    if (!eng) {
        std::cout << "COMMIT: engine not available.\n";
        return;
    }

    bool all = false;
    bool interactive_rebuild = false;

    for (std::string tok; in >> tok; ) {
        std::string up = tok;
        for (auto& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (up == "ALL") {
            all = true;
        } else if (up == "MANUAL" || up == "INTERACTIVE") {
            interactive_rebuild = true;
        } else if (up == "AUTO") {
            interactive_rebuild = false;
        } else {
            std::cout << "Usage: COMMIT [ALL] [MANUAL|INTERACTIVE|AUTO]\n";
            return;
        }
    }

    const bool talk = Settings::instance().talk_on.load();

    if (!all) {
        const int area0 = resolve_area0(A);
        if (area0 < 0) {
            std::cout << "COMMIT: cannot determine current area.\n";
            return;
        }
        commit_one_area(A, area0, talk, interactive_rebuild);
        return;
    }

    // COMMIT ALL
    int committed = 0;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        auto& Ai = eng->area(i);
        if (!Ai.isOpen()) continue;
        if (!dottalk::table::is_enabled(i)) continue;

        auto& tb = dottalk::table::get_tb(i);
        if (tb.empty()) continue;

        commit_one_area(Ai, i, talk, interactive_rebuild);
        ++committed;
    }

    if (committed == 0) {
        std::cout << "COMMIT ALL: no buffered changes.\n";
    }
}