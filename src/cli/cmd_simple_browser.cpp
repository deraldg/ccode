// src/cli/cmd_simple_browser.cpp
// -----------------------------------------------------------------------------
// Simple Browser ("workspace" browser) — order-aware via shared iterator,
// with an interactive record editor session.
//
// This file deliberately stays *single-table* and *DBF-centric*.
// Multi-table / tuple-stacking / relational browsing lives in Smart Browser.
//
// Usage (non-interactive):
//   WORKSPACE [FOR <expr>] [RAW|PRETTY] [PAGE <n>] [ALL] [TOP|BOTTOM]
//             [START KEY <literal>] [QUIET]
//
// Usage (interactive editor session):
//   WORKSPACE ... [EDIT|SESSION]
//
// Interactive keys:
//   N/P  next/prev   |  E edit field   | SAVE/CANCEL
//   DEL/RECALL       |  G <recno>      | CF (CHECK FOR)
//   R refresh order  |  ? help         | Q quit
//
// Notes:
// - Respects active order through shared order_iterator (INX/CNX/CDX).
// - START KEY uses existing SEEK (respects current order).
// - FOR supports:
//     * classic  FOR <field> <op> <value>
//     * richer boolean/algebraic expressions via where_eval
// - Deleted rows are hidden by default (future: wire to SET DELETED).
// - Interactive edit uses DbArea::set(1-based) + writeCurrent(); staging per-record.
// - After SAVE/DEL/RECALL, we rebuild the order vector and re-sync the cursor.
// - Non-interactive listing restores the original cursor position on exit.
// -----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "xbase_field_getters.hpp"
#include "predicates.hpp"
#include "textio.hpp"
#include "cli/where_eval_shared.hpp"
#include "cli/order_state.hpp"
#include "cli/order_nav.hpp"
#include "cli/order_iterator.hpp"
#include "xindex/order_display.hpp"   // namespace orderdisplay { std::string summarize(const xbase::DbArea&) }
#include "index_summary.hpp"
#include "value_normalize.hpp"        // normalize_for_compare (pre-save validation)

using std::string;
using std::vector;

using namespace util; // normalize_for_compare

// Reuse existing commands
void cmd_SEEK(xbase::DbArea& area, std::istringstream& in);
void cmd_RECALL(xbase::DbArea& area, std::istringstream& in);

namespace {

struct CursorRestore {
    xbase::DbArea* a{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& area) : a(&area) {
        saved = area.recno();
        active = (saved >= 1 && saved <= area.recCount());
    }
    void cancel() noexcept { active = false; }
    ~CursorRestore() {
        if (!active || !a) return;
        try {
            a->gotoRec(saved);
            a->readCurrent();
        } catch (...) {
            // best-effort
        }
    }
};

enum class DelFilter {
    Any,          // default: hide deleted unless ALL
    OnlyDeleted,  // show only deleted
    OnlyAlive     // show only non-deleted
};

enum class StartPos { TOP, BOTTOM };

struct BrowserOptions {
    bool want_raw{false};
    bool list_all{false};
    bool quiet{false};
    bool interactive{false};
    bool debug{false};
    int page_size{20};
    StartPos start_pos{StartPos::TOP};

    DelFilter del{DelFilter::Any};

    // Classic FOR <field> <op> <value>
    bool haveFieldFilter{false};
    string fld, op, val;

    // Advanced boolean / AST-style FOR expression
    bool haveExpr{false};
    string for_expr;

    string start_key;
};

static inline string up(string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static inline bool ieq(const string& a, const string& b) { return up(a) == up(b); }

static vector<string> tokenize(std::istringstream& in) {
    vector<string> out;
    string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

static bool parse_int(const string& s, int& out) {
    if (s.empty()) return false;
    char* e = nullptr;
    long v = std::strtol(s.c_str(), &e, 10);
    if (e == s.c_str() || *e != '\0') return false;
    if (v < INT_MIN || v > INT_MAX) return false;
    out = static_cast<int>(v);
    return true;
}

static inline bool contains_bool_words(const string& s) {
    const string u = up(" " + s + " ");
    return u.find(" AND ") != string::npos
        || u.find(" OR ")  != string::npos
        || u.find(" NOT ") != string::npos;
}

static BrowserOptions parse_browser_options(const vector<string>& t) {
    BrowserOptions o{};
    const int n = static_cast<int>(t.size());

    auto stop_for = [](const string& s) {
        return ieq(s, "RAW") || ieq(s, "PRETTY") || ieq(s, "PAGE") || ieq(s, "ALL") ||
               ieq(s, "TOP") || ieq(s, "BOTTOM") || ieq(s, "START") || ieq(s, "QUIET") ||
               ieq(s, "EDIT") || ieq(s, "SESSION") || ieq(s, "DEBUG");
    };

    // Parse simple switches first.
    for (int i = 0; i < n; ++i) {
        const string u = up(t[i]);
        if (u == "RAW") { o.want_raw = true; continue; }
        if (u == "PRETTY") { o.want_raw = false; continue; }
        if (u == "ALL") { o.list_all = true; continue; }
        if (u == "TOP") { o.start_pos = StartPos::TOP; continue; }
        if (u == "BOTTOM") { o.start_pos = StartPos::BOTTOM; continue; }
        if (u == "QUIET") { o.quiet = true; continue; }
        if (u == "DEBUG") { o.debug = true; continue; }
        if ((u == "PAGE") && i + 1 < n) {
            int page = 0;
            if (parse_int(t[i + 1], page) && page > 0) {
                o.page_size = page;
                ++i;
            }
            continue;
        }
        if ((u == "START") && i + 2 < n && ieq(t[i + 1], "KEY")) {
            o.start_key = t[i + 2];
            i += 2;
            continue;
        }
        if (u == "EDIT" || u == "SESSION") {
            o.interactive = true;
            continue;
        }
    }

    // Parse FOR tail.
    int for_pos = -1;
    for (int i = 0; i < n; ++i) {
        if (ieq(t[i], "FOR")) {
            for_pos = i;
            break;
        }
    }

    if (for_pos >= 0 && for_pos + 1 < n) {
        std::ostringstream expr_stream;
        for (int k = for_pos + 1; k < n; ++k) {
            if (stop_for(t[k])) break;
            if (k > for_pos + 1) expr_stream << ' ';
            expr_stream << t[k];
        }

        string cleaned = textio::trim(expr_stream.str());

        if (!cleaned.empty()) {
            // Special deleted shortcuts first.
            if (ieq(cleaned, "DELETED")) {
                o.del = DelFilter::OnlyDeleted;
            } else if (!cleaned.empty() &&
                       (cleaned[0] == '!' || cleaned[0] == '~') &&
                       ieq(cleaned.substr(1), "DELETED")) {
                o.del = DelFilter::OnlyAlive;
            } else {
                // SMARTLIST-style rule:
                // classic triplet only when it really looks simple;
                // otherwise preserve advanced expression engine.
                std::istringstream probe(cleaned);
                string fld, op;
                if (probe >> fld >> op) {
                    string rhs;
                    std::getline(probe, rhs);
                    rhs = textio::trim(rhs);

                    const bool rhs_has_bool = contains_bool_words(rhs);
                    const bool whole_has_bool = contains_bool_words(cleaned);

                    if (!fld.empty() && !op.empty() && !rhs.empty()
                        && !rhs_has_bool && !whole_has_bool) {
                        o.haveFieldFilter = true;
                        o.fld = fld;
                        o.op  = op;
                        o.val = rhs;
                    } else {
                        o.haveExpr = true;
                        o.for_expr = cleaned;
                    }
                } else {
                    o.haveExpr = true;
                    o.for_expr = cleaned;
                }
            }
        }
    }

    return o;
}

static void print_tuple_pretty(xbase::DbArea& db) {
    const auto& defs = db.fields();
    std::ostringstream line;
    line << "; TUPLE: ";
    for (size_t i = 0; i < defs.size(); ++i) {
        if (i) line << " | ";
        line << db.get(static_cast<int>(i) + 1);
    }
    std::cout << line.str() << "\n";
}

static void print_tuple_raw(xbase::DbArea& db) {
    const auto& defs = db.fields();
    std::ostringstream line;
    for (size_t i = 0; i < defs.size(); ++i) line << db.get(static_cast<int>(i) + 1);
    std::cout << line.str() << "\n";
}

static bool hide_deleted_by_default() { return true; }

static bool pass_deleted_filter(xbase::DbArea& db, DelFilter mode) {
    const bool isDel = db.isDeleted();
    switch (mode) {
        case DelFilter::OnlyDeleted: return isDel;
        case DelFilter::OnlyAlive:   return !isDel;
        case DelFilter::Any:
        default:                     return hide_deleted_by_default() ? !isDel : true;
    }
}

static bool more_prompt(bool quiet) {
    if (quiet) return true;
    std::cout << "-- More -- (Enter to continue, Q to quit) ";
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    if (!line.empty() && (line[0] == 'q' || line[0] == 'Q')) return false;
    return true;
}

// Use the proven utility banner that already shows INX/CNX details (including tag)
static std::string order_banner(xbase::DbArea& a) { return orderdisplay::summarize(a); }

// Active order snapshot (recno list + direction)
struct Ordered {
    vector<uint64_t> recnos;   // ascending recnos in active order
    bool asc = true;           // current direction from orderstate / iterator spec
    bool has = false;
    cli::OrderIterSpec spec{};
    string err;
};

static Ordered build_order_vector(xbase::DbArea& area) {
    Ordered o;
    std::vector<uint64_t> recnos;
    cli::OrderIterSpec spec{};
    std::string err;

    const bool ok = cli::order_collect_recnos_asc(area, recnos, &spec, &err);

    o.recnos = std::move(recnos);
    o.asc    = spec.ascending;
    o.has    = ok && !o.recnos.empty();
    o.spec   = spec;
    o.err    = err;
    return o;
}

static bool position_to_recno(xbase::DbArea& area, uint64_t rn) {
    if (!area.gotoRec(static_cast<int32_t>(rn))) return false;
    return area.readCurrent();
}

static void print_help_inline() {
    std::cout <<
        "Commands:\n"
        "  N / P          - Next / Previous (order-aware, respects FOR)\n"
        "  G <recno>      - Go to record number\n"
        "  R              - Refresh (rebuild active order vector + re-sync cursor)\n"
        "  E [<field> [WITH <value>]]  - Edit current record (prompt if <field> omitted)\n"
        "  SAVE / CANCEL  - Commit or discard staged edits\n"
        "  DEL / RECALL   - Mark deleted / Undelete current record\n"
        "  CF (CHECK FOR) <expr> - Evaluate FOR on current record (TRUE/FALSE)\n"
        "  STATUS         - Reprint status line\n"
        "  H / ?          - Help\n"
        "  Q              - Quit\n";
}

static void banner_order_updated() {
    std::cout << "[ORDER UPDATED] Active order rebuilt and cursor re-synced.\n";
}

// --- CHECK FOR + pre-save validation ---

static bool browse_eval_for_on_current(xbase::DbArea& area,
                                       const BrowserOptions& opts,
                                       bool& ok) {
    ok = false;
    if (!area.readCurrent()) return false;

    if (!pass_deleted_filter(area, opts.del)) {
        ok = true;
        return false;
    }

    if (opts.haveFieldFilter) {
        ok = true;
        return predicates::eval(area, opts.fld, opts.op, opts.val);
    }

    if (opts.haveExpr) {
        auto prog = where_eval::compile_where_expr_cached(opts.for_expr);
        if (!prog || !prog->plan) return false;
        ok = true;
        return where_eval::run_program(*prog->plan, area);
    }

    ok = true;
    return true;
}

static bool browse_validate_staged_before_save(xbase::DbArea& area,
                                               const std::map<int, std::string>& staged) {
    if (staged.empty()) return true;

    const auto& defs = area.fields();
    for (const auto& kv : staged) {
        int idx = kv.first;
        if (idx < 1 || idx > static_cast<int>(defs.size())) {
            std::cout << "SAVE blocked: invalid field index #" << idx << "\n";
            return false;
        }

        const auto& f = defs[static_cast<size_t>(idx) - 1];
        const char t  = f.type;
        const int  L  = static_cast<int>(f.length);
        const int  D  = static_cast<int>(f.decimals);

        auto norm = normalize_for_compare(t, L, D, kv.second);
        if (!norm) {
            std::cout << "SAVE blocked: invalid value for " << f.name << "\n";
            return false;
        }
        if ((t == 'C' || t == 'N') && static_cast<int>(norm->size()) > L) {
            std::cout << "SAVE blocked: value too wide for " << f.name
                      << " (" << norm->size() << " > " << L << ")\n";
            return false;
        }
    }
    return true;
}

} // namespace

void cmd_SIMPLE_BROWSER(xbase::DbArea& area, std::istringstream& in)
{
    if (!area.isOpen()) { std::cout << "WORKSPACE: no file open.\n"; return; }

    CursorRestore restore(area); // non-interactive listing restores cursor on exit

    const auto toks = tokenize(in);
    const BrowserOptions opts = parse_browser_options(toks);

    if (opts.interactive) {
        // Interactive session is expected to leave you at the final cursor position.
        restore.cancel();
    }

    if (!opts.quiet) {
        std::cout << "Entered WORKSPACE mode " << (opts.interactive ? "(interactive)" : "(read-only)") << ".\n";
        std::cout << "ORDER: " << order_banner(area) << "\n";
        std::cout << "Format: " << (opts.want_raw ? "RAW" : "PRETTY")
                  << " | Start: " << (opts.start_pos == StartPos::TOP ? "TOP" : "BOTTOM")
                  << " | Page: " << opts.page_size
                  << (opts.list_all ? " | ALL" : "")
                  << (opts.haveFieldFilter ? (" | FOR: " + opts.fld + " " + opts.op + " " + opts.val) : "")
                  << (opts.haveExpr ? (" | FOR: " + opts.for_expr) : "")
                  << (opts.start_key.empty() ? "" : " | START KEY: " + opts.start_key)
                  << "\n\n";
    }

    // Compile advanced FOR once.
    std::shared_ptr<const where_eval::CacheEntry> prog;
    if (opts.haveExpr) {
        prog = where_eval::compile_where_expr_cached(opts.for_expr);
        if (!prog) {
            if (!opts.quiet) std::cout << "Invalid FOR expression.\n";
            return;
        }
    }

    // Starting position
    if (opts.start_pos == StartPos::TOP) area.top(); else area.bottom();
    area.readCurrent();

    // START KEY uses existing SEEK (respects current order)
    if (!opts.start_key.empty()) {
        std::istringstream seek_args(opts.start_key);
        cmd_SEEK(area, seek_args);
        area.readCurrent();
    }

    auto print_tuple = [&](xbase::DbArea& db) {
        if (opts.want_raw) print_tuple_raw(db);
        else print_tuple_pretty(db);
    };

    // Build order vector (if any)
    auto ord = build_order_vector(area);

    // Helper: visibility & filter match.
    // Caller is responsible for positioning + reading current record first.
    auto visible_match = [&](xbase::DbArea& db) -> bool {
        if (!pass_deleted_filter(db, opts.del)) return false;
        if (opts.haveFieldFilter && !predicates::eval(db, opts.fld, opts.op, opts.val)) return false;
        if (prog && !where_eval::run_program(*prog->plan, db)) return false;
        return true;
    };

    // --------- Non-interactive listing ---------
    if (!opts.interactive) {
        const long limit = opts.list_all ? std::numeric_limits<long>::max() : opts.page_size;
        long shown_this_page = 0, total = 0;

        if (ord.has && !ord.recnos.empty()) {
            auto emit = [&](uint64_t rn) -> bool {
                if (!position_to_recno(area, rn)) return true;
                if (!visible_match(area)) return true;
                print_tuple(area);
                ++shown_this_page;
                ++total;
                if (shown_this_page >= limit && !opts.list_all) {
                    shown_this_page = 0;
                    return more_prompt(opts.quiet);
                }
                return true;
            };

            if (ord.asc) {
                if (opts.start_pos == StartPos::TOP) {
                    for (size_t i = 0; i < ord.recnos.size(); ++i) {
                        if (!emit(ord.recnos[i])) break;
                    }
                } else {
                    for (size_t k = ord.recnos.size(); k-- > 0;) {
                        if (!emit(ord.recnos[k])) break;
                    }
                }
            } else { // DESC
                if (opts.start_pos == StartPos::TOP) {
                    for (size_t k = ord.recnos.size(); k-- > 0;) {
                        if (!emit(ord.recnos[k])) break;
                    }
                } else {
                    for (size_t i = 0; i < ord.recnos.size(); ++i) {
                        if (!emit(ord.recnos[i])) break;
                    }
                }
            }

            if (!opts.quiet) std::cout << total << " record(s) listed.\n";
            return;
        }

        // Physical fallback
        const int step = (opts.start_pos == StartPos::TOP) ? +1 : -1;

        for (;;) {
            const int rn = area.recno(), rct = area.recCount();
            if (rn < 1 || rn > rct) break;

            if (visible_match(area)) {
                print_tuple(area);
                ++shown_this_page;
                ++total;
                if (shown_this_page >= limit && !opts.list_all) {
                    shown_this_page = 0;
                    if (!more_prompt(opts.quiet)) break;
                }
            }

            if (!area.skip(step)) break;
            area.readCurrent();
        }

        if (!opts.quiet) std::cout << total << " record(s) listed.\n";
        return;
    }

    // --------- Interactive session (EDIT/SESSION) ---------

    auto find_index_of_recno = [&](const vector<uint64_t>& v, uint64_t rn) -> int {
        for (size_t i = 0; i < v.size(); ++i) if (v[i] == rn) return static_cast<int>(i);
        return -1;
    };

    int cur_idx = -1;
    if (ord.has && !ord.recnos.empty()) {
        cur_idx = find_index_of_recno(ord.recnos, static_cast<uint64_t>(area.recno()));
        if (cur_idx < 0) {
            cur_idx = (opts.start_pos == StartPos::TOP) ? 0 : static_cast<int>(ord.recnos.size()) - 1;
            position_to_recno(area, ord.recnos[static_cast<size_t>(cur_idx)]);
        }
    }

    auto rebuild_and_resync_order = [&]() {
        if (!orderstate::hasOrder(area)) return; // physical only: nothing to do
        const int cur_rec = area.recno();

        ord = build_order_vector(area);

        if (!ord.recnos.empty()) {
            int idx = find_index_of_recno(ord.recnos, static_cast<uint64_t>(cur_rec));
            if (idx < 0) {
                bool moved = false;
                int try_idx_first  = ord.asc ? 0 : static_cast<int>(ord.recnos.size()) - 1;
                int try_idx_second = ord.asc ? static_cast<int>(ord.recnos.size()) - 1 : 0;

                for (int candidate : {try_idx_first, try_idx_second}) {
                    if (position_to_recno(area, ord.recnos[static_cast<size_t>(candidate)]) &&
                        visible_match(area)) {
                        cur_idx = candidate;
                        moved = true;
                        break;
                    }
                }
                if (!moved) {
                    for (size_t i = 0; i < ord.recnos.size(); ++i) {
                        if (position_to_recno(area, ord.recnos[i]) && visible_match(area)) {
                            cur_idx = static_cast<int>(i);
                            moved = true;
                            break;
                        }
                    }
                }
                if (!moved) cur_idx = -1;
            } else {
                cur_idx = idx;
                position_to_recno(area, ord.recnos[static_cast<size_t>(cur_idx)]);
            }
        }
        banner_order_updated();
    };

    // Staging map: field index (1-based) -> staged value
    std::map<int, string> staged;
    auto dirty = [&]() { return !staged.empty(); };

    auto print_status = [&]() {
        std::string tab;
        try {
            tab = area.name();
            if (tab.empty()) tab = area.filename();
        } catch (...) {
            tab.clear();
        }
        if (tab.empty()) tab = "(no table)";

        std::cout
            << "Table: " << tab
            << " | Recs: " << area.recCount()
            << " | Recno: " << area.recno()
            << " | Order: " << order_banner(area)
            << (dirty() ? " | *DIRTY*" : "")
            << (area.isDeleted() ? " | [DELETED]" : "")
            << "\n";
    };

    auto show_current = [&]() {
        if (!area.readCurrent()) {
            std::cout << "(unreadable record)\n";
            return;
        }
        if (opts.want_raw) print_tuple_raw(area);
        else print_tuple_pretty(area);
    };

    auto next_in_order = [&]() -> bool {
        if (ord.has && !ord.recnos.empty()) {
            int step = ord.asc ? +1 : -1;
            if (cur_idx < 0) cur_idx = (ord.asc ? 0 : static_cast<int>(ord.recnos.size()) - 1);
            for (;;) {
                cur_idx += step;
                if (cur_idx < 0 || cur_idx >= static_cast<int>(ord.recnos.size())) return false;
                if (!position_to_recno(area, ord.recnos[static_cast<size_t>(cur_idx)])) continue;
                if (visible_match(area)) return true;
            }
        } else {
            while (area.skip(+1)) {
                area.readCurrent();
                if (visible_match(area)) return true;
            }
            return false;
        }
    };

    auto prev_in_order = [&]() -> bool {
        if (ord.has && !ord.recnos.empty()) {
            int step = ord.asc ? -1 : +1;
            if (cur_idx < 0) cur_idx = (ord.asc ? 0 : static_cast<int>(ord.recnos.size()) - 1);
            for (;;) {
                cur_idx += step;
                if (cur_idx < 0 || cur_idx >= static_cast<int>(ord.recnos.size())) return false;
                if (!position_to_recno(area, ord.recnos[static_cast<size_t>(cur_idx)])) continue;
                if (visible_match(area)) return true;
            }
        } else {
            while (area.skip(-1)) {
                area.readCurrent();
                if (visible_match(area)) return true;
            }
            return false;
        }
    };

    auto commit_staged = [&]() -> bool {
        if (staged.empty()) return true;
        for (auto& kv : staged) {
            if (!area.set(kv.first, kv.second)) {
                std::cout << "Failed to set field #" << kv.first << "\n";
                return false;
            }
        }
        if (!area.writeCurrent()) {
            std::cout << "Failed to write record.\n";
            return false;
        }
        staged.clear();
        return true;
    };

    auto discard_staged = [&]() { staged.clear(); };

    auto list_fields = [&]() {
        if (!area.readCurrent()) {
            std::cout << "(unreadable record)\n";
            return;
        }
        const auto& defs = area.fields();
        std::cout << "Fields (" << defs.size() << ")\n";
        for (size_t i = 0; i < defs.size(); ++i) {
            const auto& f = defs[i];
            std::cout << "  " << (i + 1) << ": " << f.name << " [" << f.type << "]";
            if (f.type == 'C' || f.type == 'N') std::cout << " len=" << static_cast<int>(f.length);
            if (f.type == 'N' && f.decimals > 0) std::cout << " dec=" << static_cast<int>(f.decimals);
            std::cout << "  current=" << area.get(static_cast<int>(i) + 1) << "\n";
        }
    };

    auto field_index_by_name = [&](const string& name) -> int {
        const auto& defs = area.fields();
        string needle = up(name);
        for (size_t i = 0; i < defs.size(); ++i) {
            if (up(defs[i].name) == needle) return static_cast<int>(i) + 1;
        }
        return 0;
    };

    auto edit_field = [&](int field_idx, const string* value_opt) -> void {
        const auto& defs = area.fields();
        if (field_idx < 1 || field_idx > static_cast<int>(defs.size())) {
            std::cout << "Invalid field.\n";
            return;
        }
        const auto& f = defs[static_cast<size_t>(field_idx) - 1];

        string newval;
        if (value_opt) {
            newval = *value_opt;
        } else {
            std::cout << "Enter value for " << f.name << " [" << f.type
                      << "] (current=" << area.get(field_idx) << "): ";
            std::getline(std::cin, newval);
        }

        staged[field_idx] = newval;
        std::cout << "Staged " << f.name << " = " << newval << "\n";
    };

    // Initial display
    print_status();
    show_current();
    print_help_inline();

    for (;;) {
        std::cout << "WS> ";
        std::cout.flush();

        string line;
        if (!std::getline(std::cin, line)) break;

        std::istringstream lin(line);
        string cmd;
        lin >> cmd;
        if (cmd.empty()) continue;

        string CMD = up(cmd);

        if (CMD == "Q" || CMD == "QUIT") {
            if (dirty()) {
                std::cout << "You have staged changes. SAVE or CANCEL first.\n";
                continue;
            }
            break;
        }

        if (CMD == "H" || CMD == "?") {
            print_help_inline();
            continue;
        }

        if (CMD == "STATUS") {
            print_status();
            continue;
        }

        if (CMD == "CF" || CMD == "CHECK") {
            // Accept:
            //   CHECK FOR <expr>
            //   CF <expr>
            //
            // Preserve classic compatibility only when the expression is
            // truly simple; otherwise keep the richer engine.
            string expr;
            std::getline(lin, expr);
            if (!expr.empty() && expr[0] == ' ') expr.erase(0, 1);

            if (expr.size() >= 4 && up(expr.substr(0, 4)) == "FOR ") {
                expr = expr.substr(4);
                if (!expr.empty() && expr[0] == ' ') expr.erase(0, 1);
            }

            BrowserOptions cf_opts = opts;
            cf_opts.haveExpr = false;
            cf_opts.haveFieldFilter = false;
            cf_opts.for_expr.clear();
            cf_opts.fld.clear();
            cf_opts.op.clear();
            cf_opts.val.clear();

            expr = textio::trim(expr);
            if (ieq(expr, "DELETED")) {
                cf_opts.del = DelFilter::OnlyDeleted;
            } else if (!expr.empty() &&
                       (expr[0] == '!' || expr[0] == '~') &&
                       ieq(expr.substr(1), "DELETED")) {
                cf_opts.del = DelFilter::OnlyAlive;
            } else {
                std::istringstream probe(expr);
                string fld, op;
                if ((probe >> fld >> op)) {
                    string rhs;
                    std::getline(probe, rhs);
                    rhs = textio::trim(rhs);

                    if (!fld.empty() && !op.empty() && !rhs.empty()
                        && !contains_bool_words(rhs) && !contains_bool_words(expr)) {
                        cf_opts.haveFieldFilter = true;
                        cf_opts.fld = fld;
                        cf_opts.op  = op;
                        cf_opts.val = rhs;
                    } else {
                        cf_opts.haveExpr = true;
                        cf_opts.for_expr = expr;
                    }
                } else if (!expr.empty()) {
                    cf_opts.haveExpr = true;
                    cf_opts.for_expr = expr;
                }
            }

            bool ok = false;
            bool truth = browse_eval_for_on_current(area, cf_opts, ok);
            if (!ok) std::cout << "CHECK FOR: invalid or unreadable.\n";
            else     std::cout << "CHECK FOR => " << (truth ? "TRUE" : "FALSE") << "\n";
            continue;
        }

        if (CMD == "R" || CMD == "REFRESH") {
            if (dirty()) {
                std::cout << "*DIRTY* ? SAVE or CANCEL before refresh.\n";
                continue;
            }
            rebuild_and_resync_order();
            print_status();
            show_current();
            continue;
        }

        if (CMD == "N") {
            if (dirty()) { std::cout << "*DIRTY* ? SAVE or CANCEL before moving.\n"; continue; }
            if (next_in_order()) { print_status(); show_current(); }
            else { std::cout << "(end)\n"; }
            continue;
        }

        if (CMD == "P") {
            if (dirty()) { std::cout << "*DIRTY* ? SAVE or CANCEL before moving.\n"; continue; }
            if (prev_in_order()) { print_status(); show_current(); }
            else { std::cout << "(begin)\n"; }
            continue;
        }

        if (CMD == "G") {
            if (dirty()) { std::cout << "*DIRTY* ? SAVE or CANCEL before moving.\n"; continue; }

            string rn_s;
            lin >> rn_s;
            int rn = 0;
            if (!parse_int(rn_s, rn) || rn < 1) {
                std::cout << "Usage: G <recno>\n";
                continue;
            }
            if (!position_to_recno(area, static_cast<uint64_t>(rn))) {
                std::cout << "Bad recno.\n";
                continue;
            }

            if (ord.has && !ord.recnos.empty()) {
                int idx = find_index_of_recno(ord.recnos, static_cast<uint64_t>(rn));
                if (idx >= 0) cur_idx = idx; // keep order-aware navigation consistent after G
            }

            print_status();
            show_current();
            continue;
        }

        if (CMD == "E") {
            // E <field> [WITH <value>]
            string field;
            lin >> field;

            if (field.empty()) {
                list_fields();
                std::cout << "Field name or #? ";
                string pick;
                std::getline(std::cin, pick);

                int fidx = 0;
                if (parse_int(pick, fidx)) {
                    // numeric index already in fidx
                } else {
                    fidx = field_index_by_name(pick);
                }

                if (fidx <= 0) { std::cout << "Invalid selection.\n"; continue; }
                edit_field(fidx, nullptr);
            } else {
                int fidx = 0;
                if (!parse_int(field, fidx)) fidx = field_index_by_name(field);
                if (fidx <= 0) { std::cout << "Unknown field.\n"; continue; }

                string with;
                lin >> with;

                string value;
                if (!with.empty() && up(with) == "WITH") {
                    std::getline(lin, value);
                    if (!value.empty() && value[0] == ' ') value.erase(0, 1);
                } else {
                    std::cout << "Enter value: ";
                    std::getline(std::cin, value);
                }

                edit_field(fidx, &value);
            }
            continue;
        }

        if (CMD == "SAVE") {
            if (!browse_validate_staged_before_save(area, staged)) continue;
            if (commit_staged()) {
                std::cout << "Saved.\n";
                rebuild_and_resync_order();
                show_current();
            }
            continue;
        }

        if (CMD == "CANCEL") {
            discard_staged();
            std::cout << "Canceled staged edits.\n";
            continue;
        }

        if (CMD == "DEL") {
            if (dirty()) { std::cout << "*DIRTY* ? SAVE or CANCEL first.\n"; continue; }

            if (area.deleteCurrent()) {
                std::cout << "Deleted.\n";
                rebuild_and_resync_order();
                if (!next_in_order()) std::cout << "(end)\n";
                else { print_status(); show_current(); }
            } else {
                std::cout << "Delete failed.\n";
            }
            continue;
        }

        if (CMD == "RECALL" || CMD == "UNDELETE") {
            if (dirty()) { std::cout << "*DIRTY* ? SAVE or CANCEL first.\n"; continue; }

            bool wasDel = area.isDeleted();
            std::istringstream none;
            cmd_RECALL(area, none);

            bool ok = wasDel && !area.isDeleted();
            if (ok) {
                std::cout << "Recalled.\n";
                rebuild_and_resync_order();
                print_status();
                show_current();
            } else {
                std::cout << "Recall failed.\n";
            }
            continue;
        }

        std::cout << "Unknown command. Type ? for help.\n";
    }
}