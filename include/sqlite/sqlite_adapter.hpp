#pragma once
// sqlite_adapter.hpp
// Minimal SQLite wrapper for DotTalk++.
//
// Goals:
//  - Keep SQLite C headers out of most translation units.
//  - Provide a small, stable surface for cmd_sqlite.cpp and future import/export.
//  - Avoid exceptions across module boundaries; return bool + error string.
//
// Build flag:
//   - Define DOTTALK_SQLITE_AVAILABLE=1 when sqlite3 is linked.

#include <functional>
#include <string>
#include <vector>

namespace dottalk::sqlite {

struct SqliteDb {
    void* handle{nullptr}; // sqlite3*
};

using Row  = std::vector<std::string>;
using Cols = std::vector<std::string>;

// Open/Close
bool sqlite_open(SqliteDb& db, const std::string& path, std::string& err);
void sqlite_close(SqliteDb& db) noexcept;

// Execute a non-SELECT statement (CREATE/INSERT/UPDATE/DELETE, etc.)
bool sqlite_exec(SqliteDb& db, const std::string& sql, std::string& err);

// Query rows. Calls on_row for each row. Returns false on any SQLite error.
//
// Notes:
//  - The Cols vector is stable across all callbacks for a single query.
//  - Values are returned as UTF-8 text via sqlite3_column_text().
//    NULL -> empty string.
bool sqlite_query_cols(
    SqliteDb& db,
    const std::string& sql,
    const std::function<void(const Cols& cols, const Row& row)>& on_row,
    std::string& err);

// Back-compat (no columns)
bool sqlite_query(
    SqliteDb& db,
    const std::string& sql,
    const std::function<void(const Row& row)>& on_row,
    std::string& err);

// Introspection
bool sqlite_is_open(const SqliteDb& db) noexcept;
std::string sqlite_version();

} // namespace dottalk::sqlite
