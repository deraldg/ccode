// src/cli/cmd_sql.cpp
// SQL command ? COUNT with optional ALL|DELETED and FOR <expr>.
// Uses shared DotTalk evaluator + LRU cache (DOTTALK_WHERECACHE, default 256).
//
// Change: Suppress "false" per-record logs by default. Print per-record lines
// only for matches (ok==true). Use VERBOSE to print all (true/false) details.

#include "xbase.hpp"
#include "xbase_field_getters.hpp"
#include "record_view.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "cli/expr/api.hpp"
#include "cli/expr/for_parser.hpp"
#include "expr/sql_normalize.hpp"
#include "cli/where_eval_shared.hpp"  // shared evaluator + env/LRU cache

using where_eval::dt_trim;
using where_eval::dt_upcase;

namespace { // ---------- local helpers ----------

static inline std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

// Remove standalone token (case-insensitive) from a string. Returns (new, removed?)
static std::pair<std::string,bool> strip_token_ci(const std::string& text, const std::string& token) {
    std::istringstream ss(text);
    std::ostringstream out;
    std::string w;
    bool removed = false;
    bool first = true;
    const std::string TOKEN = up(token);
    while (ss >> w) {
        if (up(w) == TOKEN) { removed = true; continue; }
        if (!first) out << ' ';
        out << w;
        first = false;
    }
    return { out.str(), removed };
}

// Deleted-record handling
enum class DelMode { SkipDeleted, OnlyDeleted, IncludeAll };

struct Opts {
    DelMode     mode    = DelMode::SkipDeleted;
    bool        haveFor = false;
    bool        verbose = false;
    std::string forRaw;    // expression after FOR, or whole expr if no "FOR"
    std::string tailRaw;   // everything after SQL keyword (for debug echo)
};

// Parse: SQL [VERBOSE] [COUNT] [ALL|DELETED] [FOR <expr> | <expr>] [VERBOSE]
static Opts parse_opts(std::istringstream& iss) {
    Opts o;

    // Rebuild unconsumed tail from the stream
    std::string rest;
    {
        const std::string& all = iss.str();
        auto pos = iss.tellg();
        if (pos != std::istringstream::pos_type(-1)) {
            size_t i = static_cast<size_t>(pos);
            if (i < all.size()) rest = all.substr(i);
        } else {
            rest = all;
        }
    }
    rest = dt_trim(rest);

    // Allow VERBOSE anywhere: first strip once, then again after we reshape.
    {
        auto [t1, found1] = strip_token_ci(rest, "VERBOSE");
        o.verbose = o.verbose || found1;
        rest = t1;
    }

    o.tailRaw = dt_trim(rest);

    std::istringstream head(o.tailRaw);

    // Optional COUNT
    std::streampos afterFirst = head.tellg();
    std::string t;
    if (head >> t) {
        if (up(t) != "COUNT") {
            head.clear();
            head.seekg(afterFirst);
        }
    }

    // Optional ALL | DELETED
    std::streampos afterMode = head.tellg();
    std::string modeTok;
    if (head >> modeTok) {
        const auto M = up(modeTok);
        if      (M == "ALL")     o.mode = DelMode::IncludeAll;
        else if (M == "DELETED") o.mode = DelMode::OnlyDeleted;
        else { head.clear(); head.seekg(afterMode); }
    }

    // Remaining text as potential FOR or raw expr
    std::string tail; std::getline(head, tail);
    tail = dt_trim(tail);

    // Permit a trailing VERBOSE as well
    {
        auto [t2, found2] = strip_token_ci(tail, "VERBOSE");
        o.verbose = o.verbose || found2;
        tail = dt_trim(t2);
    }

    if (!tail.empty()) {
        auto U = up(tail);
        if (U.rfind("FOR", 0) == 0) {
            o.haveFor = true;
            o.forRaw = dt_trim(tail.substr(3));
        } else {
            o.haveFor = true;
            o.forRaw = tail;
        }
    }

    return o;
}

// Record filter by deleted flag
static inline bool include_row(bool deleted, DelMode mode) {
    if (mode == DelMode::SkipDeleted && deleted)  return false;
    if (mode == DelMode::OnlyDeleted && !deleted) return false;
    return true;
}

} // anon

// ---------- command ----------
void cmd_SQL(xbase::DbArea& A, std::istringstream& iss) {
    if (!A.isOpen()) { std::cout << "No file open\n"; return; }

    const Opts opt = parse_opts(iss);

    std::cout << "SQL DEBUG ? raw: \"" << opt.tailRaw << "\"\n";

    std::vector<std::string> debug_fields;
    std::shared_ptr<const where_eval::CacheEntry> ce;

    if (opt.haveFor) {
        const std::string normalized = sqlnorm::sql_to_dottalk_where(opt.forRaw);
        std::cout << "SQL DEBUG ? normalized: " << normalized << "\n";

        // Fields to print per-record when we emit lines
        debug_fields = where_eval::extract_field_names(normalized);
        if (!debug_fields.empty()) {
            std::cout << "SQL DEBUG ? fields: ";
            for (size_t i=0;i<debug_fields.size();++i) {
                if (i) std::cout << ", ";
                std::cout << debug_fields[i];
            }
            std::cout << "\n";
        } else {
            std::cout << "SQL DEBUG ? fields: (none detected)\n";
        }

        try {
            // Compile via shared env/LRU cache
            ce = where_eval::compile_where_expr_cached(opt.forRaw);
            std::cout << "SQL DEBUG ? compiled: " << where_eval::plan_kind(*ce->plan) << "\n";
        } catch (const std::exception& ex) {
            std::cout << "Syntax error in FOR: " << ex.what() << "\n";
            return;
        }
    } else {
        std::cout << "SQL DEBUG ? no clause (plain COUNT)\n";
    }

    long long cnt = 0, scanned = 0;

    if (A.top() && A.readCurrent()) {
        do {
            ++scanned;
            if (!include_row(A.isDeleted(), opt.mode)) continue;

            const bool ok = (!opt.haveFor) ? true : where_eval::run_program(*ce->plan, A);

            // Per-record output policy:
            // - VERBOSE: print every record with true/false
            // - Default: print matches only; suppress false lines entirely
            if (opt.verbose) {
                if (debug_fields.empty()) {
                    std::cout << "[rec " << A.recno() << "] => " << (ok ? "true" : "false") << "\n";
                } else {
                    std::ostringstream fv;
                    fv << "[rec " << A.recno() << "] ";
                    for (size_t i=0;i<debug_fields.size();++i) {
                        const std::string& fld = debug_fields[i];

                        std::string s;
                        try { s = xfg::getFieldAsString(A, fld); } catch (...) { s = "(ERR)"; }
                        fv << fld << "=\"" << dt_upcase(dt_trim(s)) << "\"";

                        try {
                            double n = xfg::getFieldAsNumber(A, fld);
                            if (std::isfinite(n)) fv << " (num=" << n << ")";
                        } catch (...) {}

                        if (i+1 < debug_fields.size()) fv << ", ";
                    }
                    fv << " => " << (ok ? "true" : "false");
                    std::cout << fv.str() << "\n";
                }
            } else if (ok) {
                // Non-verbose: emit only matches (or nothing if none)
                if (debug_fields.empty()) {
                    std::cout << "[rec " << A.recno() << "]\n";
                } else {
                    std::ostringstream fv;
                    fv << "[rec " << A.recno() << "] ";
                    for (size_t i=0;i<debug_fields.size();++i) {
                        const std::string& fld = debug_fields[i];

                        std::string s;
                        try { s = xfg::getFieldAsString(A, fld); } catch (...) { s = "(ERR)"; }
                        fv << fld << "=\"" << dt_upcase(dt_trim(s)) << "\"";

                        try {
                            double n = xfg::getFieldAsNumber(A, fld);
                            if (std::isfinite(n)) fv << " (num=" << n << ")";
                        } catch (...) {}

                        if (i+1 < debug_fields.size()) fv << ", ";
                    }
                    std::cout << fv.str() << "\n";
                }
            }

            if (ok) ++cnt;

        } while (A.skip(+1) && A.readCurrent());
    }

    std::cout << "SQL DEBUG ? scanned: " << scanned << "  matched: " << cnt << "\n";
    std::cout << cnt << "\n";
}



