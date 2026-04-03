// src/cli/cmd_sqlite.cpp
// DotTalk++: SQLITE command (thin wrapper around SQLite3 via sqlite_adapter)
//
// Usage:
//   SQLITE                         -> status + brief usage
//   SQLITE VERSION                 -> show linked SQLite version
//   SQLITE OPEN <file>|:memory:    -> open/connect (creates if needed)
//   SQLITE CLOSE                   -> close
//   SQLITE EXEC <sql...>           -> execute non-SELECT SQL
//   SQLITE SELECT <sql...>         -> run a SELECT and print rows
//
// Notes:
//   - Independent of DBF open/order state.
//   - SELECT output is capped (MAX_ROWS) to keep CLI responsive.

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "textio.hpp"
#include "sqlite/sqlite_adapter.hpp"

namespace {

static dottalk::sqlite::SqliteDb g_db;
static std::string g_db_path;

static inline std::string up_copy(std::string s) {
    for (auto& ch : s) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

static inline std::string ltrim_copy(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char c) { return !std::isspace(c); }));
    return s;
}

static void print_usage_brief() {
    std::cout << "SQLITE: VERSION, OPEN <file|:memory:>, CLOSE, EXEC <sql>, SELECT <sql>\n";
}

static bool ensure_open(std::string& err) {
    if (dottalk::sqlite::sqlite_is_open(g_db)) return true;

    // Default to in-memory if user runs EXEC/SELECT without OPEN.
    g_db_path = ":memory:";
    if (!dottalk::sqlite::sqlite_open(g_db, g_db_path, err)) return false;

    std::cout << "SQLITE: opened :memory: (implicit)\n";
    return true;
}

static void print_table(const std::vector<std::string>& cols,
                        const std::vector<dottalk::sqlite::Row>& rows) {
    if (cols.empty()) {
        std::cout << "(no columns)\n";
        return;
    }

    const size_t ncol = cols.size();
    std::vector<size_t> w(ncol, 0);

    for (size_t i = 0; i < ncol; ++i) w[i] = cols[i].size();
    for (const auto& r : rows) {
        for (size_t i = 0; i < ncol; ++i) {
            const std::string& cell = (i < r.size()) ? r[i] : std::string();
            if (cell.size() > w[i]) w[i] = cell.size();
        }
    }

    for (size_t i = 0; i < ncol; ++i) {
        if (i) std::cout << " | ";
        std::cout << std::left << std::setw(static_cast<int>(w[i])) << cols[i];
    }
    std::cout << "\n";

    for (size_t i = 0; i < ncol; ++i) {
        if (i) std::cout << "-+-";
        std::cout << std::string(w[i], '-');
    }
    std::cout << "\n";

    for (const auto& r : rows) {
        for (size_t i = 0; i < ncol; ++i) {
            if (i) std::cout << " | ";
            const std::string& cell = (i < r.size()) ? r[i] : std::string();
            std::cout << std::left << std::setw(static_cast<int>(w[i])) << cell;
        }
        std::cout << "\n";
    }
}

} // namespace

// ---- command entry ----
// MUST be global + MUST match shell registration signature.
void cmd_SQLITE(xbase::DbArea& /*A*/, std::istringstream& args) {
    std::string sub;
    if (!(args >> sub)) {
        std::cout << "SQLITE: "
                  << (dottalk::sqlite::sqlite_is_open(g_db) ? "open" : "closed")
                  << " (db='" << (g_db_path.empty() ? "" : g_db_path) << "')\n";
        print_usage_brief();
        return;
    }

    sub = up_copy(sub);

    if (sub == "VERSION") {
        std::cout << "SQLite: " << dottalk::sqlite::sqlite_version() << "\n";
        return;
    }

#if !defined(DOTTALK_SQLITE_AVAILABLE) || !DOTTALK_SQLITE_AVAILABLE
    std::cout << "SQLITE: not available in this build (DOTTALK_SQLITE_AVAILABLE=0)\n";
    return;
#else

    if (sub == "OPEN" || sub == "DB") {
        std::string rest;
        std::getline(args, rest);

        // textio.hpp provides trim()/unquote() (NOT trim_copy/unquote_copy)
        rest = textio::trim(rest);
        rest = textio::unquote(rest);

        if (rest.empty()) {
            std::cout << "Usage: SQLITE OPEN <file>|:memory:\n";
            return;
        }

        std::string err;
        g_db_path = rest;
        if (!dottalk::sqlite::sqlite_open(g_db, g_db_path, err)) {
            std::cout << "SQLITE OPEN failed: " << err << "\n";
            return;
        }

        std::cout << "SQLITE: opened " << g_db_path << "\n";
        return;
    }

    if (sub == "CLOSE") {
        dottalk::sqlite::sqlite_close(g_db);
        g_db_path.clear();
        std::cout << "SQLITE: closed\n";
        return;
    }

    if (sub == "EXEC") {
        std::string sql;
        std::getline(args, sql);
        sql = ltrim_copy(sql);

        if (sql.empty()) {
            std::cout << "Usage: SQLITE EXEC <sql...>\n";
            return;
        }

        std::string err;
        if (!ensure_open(err)) {
            std::cout << "SQLITE: open failed: " << err << "\n";
            return;
        }

        if (!dottalk::sqlite::sqlite_exec(g_db, sql, err)) {
            std::cout << "SQLITE EXEC failed: " << err << "\n";
            return;
        }

        std::cout << "OK\n";
        return;
    }

    if (sub == "SELECT") {
        std::string sql;
        std::getline(args, sql);
        sql = ltrim_copy(sql);

        if (sql.empty()) {
            std::cout << "Usage: SQLITE SELECT <sql...>\n";
            return;
        }

        std::string err;
        if (!ensure_open(err)) {
            std::cout << "SQLITE: open failed: " << err << "\n";
            return;
        }

        constexpr size_t MAX_ROWS = 200;
        std::vector<std::string> cols;
        std::vector<dottalk::sqlite::Row> rows;
        bool truncated = false;

        const bool ok = dottalk::sqlite::sqlite_query_cols(
            g_db,
            sql,
            [&](const std::vector<std::string>& c, const dottalk::sqlite::Row& r) {
                if (cols.empty()) cols = c;
                if (rows.size() < MAX_ROWS) rows.push_back(r);
                else truncated = true;
            },
            err
        );

        if (!ok) {
            std::cout << "SQLITE SELECT failed: " << err << "\n";
            return;
        }

        if (rows.empty()) {
            std::cout << "(no rows)\n";
        } else {
            print_table(cols, rows);
            if (truncated) std::cout << "... (truncated at " << MAX_ROWS << " rows)\n";
        }
        return;
    }

    std::cout << "SQLITE: unknown subcommand: " << sub << "\n";
    print_usage_brief();
#endif
}
