// src/cli/cmd_smartlist.cpp
//
// SMARTLIST — LIST-style output honoring current order with classic or AST-based FOR filtering.
//
// Usage:
//   SMARTLIST [ALL | <limit> | DELETED] [DEBUG] [FOR <pred>]
//
// Notes:
//   • Ordering: respects current INX/CNX/CDX/LMDB (ASC/DESC) like LIST.
//   • Deletion visibility: default hides deleted unless ALL; "DELETED"
//     shows only deleted; "FOR !DELETED" also supported.
//   • Filtering: supports classic FOR <field> <op> <value> and AST-style expressions.
//     SQL-ish input is normalized via sql_to_dottalk_where before compile.
//   • DEBUG: prints a couple of diagnostics.
//

#include "xbase.hpp"
#include "textio.hpp"
#include "predicates.hpp"
#include "filters/filter_registry.hpp"
#include "cli/order_state.hpp"
#include "cli/order_nav.hpp"
#include "cli/order_iterator.hpp"
#include "value_normalize.hpp"
#include "set_relations.hpp"

// Expr engine + helpers
#include "cli/expr/api.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/parse_utils.hpp"
#include "cli/expr/line_parse_utils.hpp"
#include "expr/sql_normalize.hpp"

// std
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

// LMDB (CDX.d pilot)
#include <lmdb.h>

namespace {

enum class DelFilter {
    Any,          // default: hide deleted unless ALL
    OnlyDeleted,  // show only deleted
    OnlyAlive     // show only non-deleted (even if ALL)
};

struct Options {
    bool all{false};
    int  limit{20};
    DelFilter del{DelFilter::Any};

    // AST/compiled FOR support (SMARTLIST should stay AST-first)
    bool haveExpr{false};
    std::string exprRaw;

    // Classic Fox-style FOR <field> <op> <value> compatibility
    bool haveFieldFilter{false};
    std::string fld, op, val;

    bool debug{false};     // set by optional DEBUG switch
};

// Preserve caller's cursor position: SMARTLIST scans by jumping around.
struct CursorRestore {
    xbase::DbArea& a;
    int32_t saved_recno{0};
    bool have{false};

    explicit CursorRestore(xbase::DbArea& area) : a(area) {
        try {
            saved_recno = a.recno();
            have = (saved_recno > 0);
        } catch (...) {
            have = false;
        }
    }

    ~CursorRestore() {
        if (!have) return;
        try {
            (void)a.gotoRec(saved_recno);
            (void)a.readCurrent();
        } catch (...) {}

        // If relations autorefresh is enabled, re-sync children for the restored parent position.
        try { relations_api::refresh_if_enabled(); } catch (...) {}
    }
};

// ---------- small utils ----------
static inline bool is_uint(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) if (!std::isdigit(c)) return false;
    return true;
}
static inline std::string trim(std::string s) {
    auto sp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static inline std::string up(std::string s) {
    for (auto& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}
static inline bool ends_with_iex(const std::string& s, const char* EXT3) {
    size_t n=s.size();
    return n>=4 && s[n-4]=='.'
        && (char)std::toupper((unsigned char)s[n-3])==EXT3[0]
        && (char)std::toupper((unsigned char)s[n-2])==EXT3[1]
        && (char)std::toupper((unsigned char)s[n-1])==EXT3[2];
}
static inline bool contains_bool_words(const std::string& s) {
    const std::string u = up(" " + s + " ");
    return u.find(" AND ") != std::string::npos
        || u.find(" OR ")  != std::string::npos
        || u.find(" NOT ") != std::string::npos;
}

// Debug print helper (suppressed unless opts.debug)
static inline void dbg(bool on, const std::string& msg) {
    if (on) std::cout << "; " << msg << "\n";
}

// Parse options in any order up to (but not including) FOR.
// Accepts: ALL | DELETED | <limit> | DEBUG
//
// SMARTLIST rule:
//   1) FOR DELETED / FOR !DELETED -> deleted visibility shortcuts
//   2) simple FOR <field> <op> <value> -> classic predicate compatibility
//   3) everything richer -> AST expression engine
static Options parse_opts(std::istringstream& iss) {
    Options o{};
    std::string tok;

    // Parse simple switches in any order up to FOR.
    while (true) {
        std::streampos save = iss.tellg();
        if (!(iss >> tok)) break;

        if (textio::ieq(tok, "FOR")) {
            iss.clear();
            iss.seekg(save);
            break;
        }
        if (textio::ieq(tok, "ALL")) {
            o.all = true;
            continue;
        }
        if (textio::ieq(tok, "DELETED")) {
            o.del = DelFilter::OnlyDeleted;
            continue;
        }
        if (textio::ieq(tok, "DEBUG")) {
            o.debug = true;
            continue;
        }
        if (is_uint(tok)) {
            o.limit = std::max(0, std::stoi(tok));
            continue;
        }

        // Unknown token: rewind and stop option parsing.
        iss.clear();
        iss.seekg(save);
        break;
    }

    // Optional: FOR ...
    {
        std::streampos save = iss.tellg();
        std::string w;
        if (iss >> w && textio::ieq(w, "FOR")) {
            std::string first;
            std::streampos after_for = iss.tellg();

            if (iss >> first) {
                // Deleted shortcuts first.
                if (textio::ieq(first, "DELETED")) {
                    o.del = DelFilter::OnlyDeleted;
                    return o;
                }
                if ((first.size() == 8 || first.size() == 9) &&
                    (first[0] == '!' || first[0] == '~') &&
                    textio::ieq(first.c_str() + 1, "DELETED")) {
                    o.del = DelFilter::OnlyAlive;
                    return o;
                }

                // Read remainder of FOR tail and strip comments before deciding mode.
                std::string rest_of_line;
                std::getline(iss, rest_of_line);
                std::string cleaned = trim(strip_line_comments(first + (rest_of_line.empty() ? "" : " " + rest_of_line)));

                // Try classic triplet only when the whole FOR tail really looks simple.
                // Anything richer falls back to SMARTLIST's AST engine.
                {
                    std::istringstream probe(cleaned);
                    std::string fld, op;
                    if (probe >> fld >> op) {
                        std::string rhs;
                        std::getline(probe, rhs);
                        rhs = textio::unquote(trim(rhs));

                        const bool rhs_has_bool = contains_bool_words(rhs);
                        const bool whole_has_bool = contains_bool_words(cleaned);

                        if (!fld.empty() && !op.empty()
                            && !rhs_has_bool && !whole_has_bool) {
                            o.haveFieldFilter = true;
                            o.fld = fld;
                            o.op  = op;
                            o.val = rhs;
                            return o;
                        }
                    }
                }

                // SMARTLIST stays AST-first for anything non-trivial.
                if (!cleaned.empty()) {
                    o.haveExpr = true;
                    o.exprRaw = cleaned;
                }
                return o;
            }

            iss.clear();
            iss.seekg(after_for);
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }

    return o;
}

static int recno_width(const xbase::DbArea& a) {
    int n = std::max(1, a.recCount()), w=0; while (n) { n/=10; ++w; } return std::max(3,w);
}

static void header(const xbase::DbArea& a, int recw) {
    const auto& Fs=a.fields();
    std::cout << "  " << std::setw(recw) << "" << " ";
    for (auto& f: Fs) std::cout << std::left << std::setw((int)f.length) << f.name << " ";
    std::cout << std::right << "\n";
}

static void row(const xbase::DbArea& a, int recw) {
    const auto& Fs=a.fields();
    std::cout << ' ' << (a.isDeleted()? '*':' ') << ' ' << std::setw(recw) << a.recno() << " ";
    for (int i=1;i<=(int)Fs.size();++i) {
        std::string s=a.get(i); int w=(int)Fs[(size_t)(i-1)].length;
        if ((int)s.size()>w) s.resize((size_t)w);
        std::cout << std::left << std::setw(w) << s << " ";
    }
    std::cout << std::right << "\n";
}

// ---------- AST-based filter program ----------

static std::shared_ptr<dottalk::expr::Expr> build_expr_program_from(const std::string& userExpr,
                                                                    bool debug=false) {
    const std::string normalized = sqlnorm::sql_to_dottalk_where(userExpr);
    const std::string cleaned    = strip_line_comments(normalized);

    auto cr = dottalk::expr::compile_where(cleaned);
    if (!cr) {
        auto raw_clean = strip_line_comments(userExpr);
        cr = dottalk::expr::compile_where(raw_clean);
    }
    if (!cr) {
        if (debug) std::cout << "; compile failed — FOR will match no records\n";
        struct AlwaysFalse final : dottalk::expr::Expr {
            bool eval(const dottalk::expr::RecordView&) const override { return false; }
        };
        return std::shared_ptr<dottalk::expr::Expr>(new AlwaysFalse{});
    }

    return std::shared_ptr<dottalk::expr::Expr>(std::move(cr.program));
}

// Deleted filter gate
static inline bool pass_deleted_filter(const xbase::DbArea& a, const Options& o) {
    const bool isDel = a.isDeleted();
    switch (o.del) {
        case DelFilter::OnlyDeleted: return isDel;
        case DelFilter::OnlyAlive:   return !isDel;
        case DelFilter::Any:
        default:                     return o.all ? true : !isDel; // default: hide deleted unless ALL
    }
}

static inline bool pass_all_filters(xbase::DbArea& a,
                                    const Options& opt,
                                    const std::shared_ptr<dottalk::expr::Expr>& expr_prog)
{
    if (!pass_deleted_filter(a, opt)) return false;

    // Persistent SET FILTER + compiled FOR expression (if any)
    if (!filter::visible(&a, expr_prog)) return false;

    // Legacy classic FOR <field> <op> <value>
    if (opt.haveFieldFilter &&
        !predicates::eval(a, opt.fld, opt.op, opt.val)) {
        return false;
    }

    return true;
}

// ---------- LMDB ordered scan (CDX.d pilot) ----------

static inline bool file_exists(const std::filesystem::path& p) {
    std::error_code ec{};
    return std::filesystem::exists(p, ec);
}

static inline std::filesystem::path lmdb_envdir_from_cdx(const std::filesystem::path& cdxPath) {
    std::filesystem::path p = cdxPath;
    p += ".d";
    return p;
}

static inline uint64_t read_le_u64(const void* p) {
    const unsigned char* b = static_cast<const unsigned char*>(p);
    uint64_t v = 0;
    v |= (uint64_t)b[0];
    v |= (uint64_t)b[1] << 8;
    v |= (uint64_t)b[2] << 16;
    v |= (uint64_t)b[3] << 24;
    v |= (uint64_t)b[4] << 32;
    v |= (uint64_t)b[5] << 40;
    v |= (uint64_t)b[6] << 48;
    v |= (uint64_t)b[7] << 56;
    return v;
}

// Iterate recnos in tag order directly from LMDB named DB (tagname).
// Returns true only if LMDB was successfully used; otherwise caller may fall back.
static bool smartlist_try_lmdb_cdx(
    const Options& opt,
    const std::string& cdxPathStr,
    const std::string& tag,
    bool asc,
    xbase::DbArea& a,
    const std::function<bool(int32_t rn, int& printed)>& process_record,
    int& printed)
{
    if (tag.empty()) return false;

    std::filesystem::path cdxPath(cdxPathStr);
    const std::filesystem::path envDir = lmdb_envdir_from_cdx(cdxPath);
    if (!file_exists(envDir / "data.mdb")) return false;

    MDB_env* env = nullptr;
    MDB_txn* txn = nullptr;
    MDB_dbi dbi = 0;
    MDB_cursor* cur = nullptr;

    auto cleanup = [&]() {
        if (cur) mdb_cursor_close(cur);
        if (txn) mdb_txn_abort(txn);
        if (env) mdb_env_close(env);
        cur = nullptr; txn = nullptr; env = nullptr;
    };

    int rc = mdb_env_create(&env);
    if (rc != MDB_SUCCESS) { cleanup(); return false; }

    (void)mdb_env_set_maxdbs(env, 1024);

    rc = mdb_env_open(env, envDir.string().c_str(), MDB_RDONLY, 0664);
    if (rc != MDB_SUCCESS) { cleanup(); return false; }

    rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
    if (rc != MDB_SUCCESS) { cleanup(); return false; }

    rc = mdb_dbi_open(txn, tag.c_str(), 0, &dbi);
    if (rc != MDB_SUCCESS) { cleanup(); return false; }

    rc = mdb_cursor_open(txn, dbi, &cur);
    if (rc != MDB_SUCCESS) { cleanup(); return false; }

    dbg(opt.debug, "CDX.d LMDB ORDER: env '" + envDir.string() + "' DB '" + tag + "' " + (asc ? "ASC" : "DESC"));

    MDB_val k{};
    MDB_val v{};
    rc = mdb_cursor_get(cur, &k, &v, asc ? MDB_FIRST : MDB_LAST);

    while (rc == MDB_SUCCESS) {
        uint64_t rn64 = 0;

        if (v.mv_size == 8) {
            rn64 = read_le_u64(v.mv_data);
        } else if (v.mv_size == 4) {
            const unsigned char* b = static_cast<const unsigned char*>(v.mv_data);
            rn64 = (uint64_t)b[0] | ((uint64_t)b[1] << 8) | ((uint64_t)b[2] << 16) | ((uint64_t)b[3] << 24);
        } else if (k.mv_size >= 8) {
            const unsigned char* b = static_cast<const unsigned char*>(k.mv_data);
            rn64 = read_le_u64(b + (k.mv_size - 8));
        }

        const int32_t rn = (rn64 > 0 && rn64 <= (uint64_t)a.recCount()) ? (int32_t)rn64 : 0;
        if (rn > 0) {
            if (!process_record(rn, printed)) break;
        }

        rc = mdb_cursor_get(cur, &k, &v, asc ? MDB_NEXT : MDB_PREV);
    }

    cleanup();
    return true;
}

} // namespace

void cmd_SMARTLIST(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    CursorRestore __restore(a);

    {
        std::streampos pos = iss.tellg();
        std::string rest;
        {
            std::ostringstream oss;
            oss << iss.rdbuf();
            rest = oss.str();
        }
        iss.clear(); iss.seekg(pos);
        if (trim(rest).empty()) {
            std::cout << "Usage: SMARTLIST [ALL | <limit> | DELETED] [DEBUG] [FOR <pred>]\n";
        }
    }

    a.top();
    (void)a.readCurrent();

    Options opt = parse_opts(iss);
    const int32_t total = a.recCount(); if (total<=0) { std::cout << "(empty)\n"; return; }

    std::shared_ptr<dottalk::expr::Expr> expr_prog;
    if (opt.haveExpr) {
        const std::string U = up(opt.exprRaw);
        if (U=="DELETED") {
            opt.del = DelFilter::OnlyDeleted;
            opt.haveExpr = false;
        } else if (U=="!DELETED" || U=="~DELETED") {
            opt.del = DelFilter::OnlyAlive;
            opt.haveExpr = false;
        } else {
            dbg(opt.debug, std::string("Expr (raw): ") + opt.exprRaw);
            expr_prog = build_expr_program_from(opt.exprRaw, opt.debug);
            dbg(opt.debug, "Expr mode: AST engine");
        }
    }
    if (opt.haveFieldFilter) {
        dbg(opt.debug, "Expr mode: classic predicate");
    }

    const int recw = recno_width(a);
    header(a, recw);

    auto process_record = [&](int32_t rn, int& printed)->bool {
        if (!a.gotoRec(rn) || !a.readCurrent()) return false;
        if (!pass_all_filters(a, opt, expr_prog)) return true;
        row(a, recw);
        ++printed;
        if (!opt.all && opt.limit>0 && printed>=opt.limit) return false;
        return true;
    };

    int printed = 0;

    auto finish_print = [&]() {
        if (!opt.all) {
            std::cout << printed << " record(s) listed (limit " << opt.limit
                      << "). Use SMARTLIST ALL to show more.\n";
        } else {
            std::cout << printed << " record(s) listed.\n";
        }
    };

    cli::OrderIterSpec iter_spec{};
    std::string iter_err;

    const bool iter_ok = cli::order_iterate_recnos(
        a,
        [&](uint64_t rn64) -> bool {
            if (rn64 == 0 || rn64 > static_cast<uint64_t>(a.recCount64())) return true;
            return process_record(static_cast<int32_t>(rn64), printed);
        },
        &iter_spec,
        &iter_err);

    if (iter_ok) {
        if (opt.debug) {
            auto backend_name = [&](const cli::OrderIterSpec& spec) -> const char* {
                switch (spec.backend) {
                    case cli::OrderBackend::Natural: return "NATURAL";
                    case cli::OrderBackend::Inx:     return "INX";
                    case cli::OrderBackend::Cnx:     return "CNX";
                    case cli::OrderBackend::Cdx:
                        return (spec.cdx_mode == cli::CdxExecMode::Lmdb) ? "CDX/LMDB" : "CDX/FALLBACK";
                    case cli::OrderBackend::Isx:     return "ISX";
                    case cli::OrderBackend::Csx:     return "CSX";
                    default:                         return "?";
                }
            };

            std::string msg = std::string("ORDER ITER: ")
                            + backend_name(iter_spec)
                            + " path '" + (iter_spec.container_path.empty() ? "(none)" : iter_spec.container_path)
                            + "' tag '" + (iter_spec.tag.empty() ? "(none)" : iter_spec.tag)
                            + "' " + (iter_spec.ascending ? "ASC" : "DESC");
            dbg(opt.debug, msg);
        }

        finish_print();
        return;
    }

    dbg(opt.debug, "ORDER ITER failed: " + iter_err);
    dbg(opt.debug, "Falling back to physical order");

    for (int32_t rn = 1; rn <= total; ++rn) {
        if (!process_record(rn, printed)) break;
    }

    finish_print();
}