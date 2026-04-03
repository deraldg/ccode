// src/sqlite/sqlite_adapter.cpp
#include "sqlite/sqlite_adapter.hpp"

#include <string>
#include <vector>

// Keep sqlite3 out of the header.
#if defined(DOTTALK_SQLITE_AVAILABLE) && DOTTALK_SQLITE_AVAILABLE
  #include <sqlite3.h>
#endif

namespace dottalk::sqlite {

#if defined(DOTTALK_SQLITE_AVAILABLE) && DOTTALK_SQLITE_AVAILABLE

static inline sqlite3* dbh(const SqliteDb& db) noexcept {
    return reinterpret_cast<sqlite3*>(db.handle);
}

static inline void set_err(std::string& err, sqlite3* db, int rc, const char* prefix) {
    const char* msg = db ? sqlite3_errmsg(db) : "(no sqlite handle)";
    err.clear();
    err += prefix;
    err += " rc=";
    err += std::to_string(rc);
    err += ": ";
    err += (msg ? msg : "(null)");
}

std::string sqlite_version() {
    return std::string(sqlite3_libversion());
}

bool sqlite_is_open(const SqliteDb& db) noexcept {
    return db.handle != nullptr;
}

bool sqlite_open(SqliteDb& db, const std::string& path, std::string& err) {
    // Close any existing db first.
    sqlite_close(db);

    sqlite3* h = nullptr;
    const int rc = sqlite3_open_v2(
        path.c_str(),
        &h,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        nullptr
    );

    if (rc != SQLITE_OK) {
        set_err(err, h, rc, "sqlite_open");
        if (h) sqlite3_close(h);
        return false;
    }

    db.handle = h;
    err.clear();
    return true;
}

void sqlite_close(SqliteDb& db) noexcept {
    if (db.handle) {
        sqlite3_close(dbh(db));
        db.handle = nullptr;
    }
}

bool sqlite_exec(SqliteDb& db, const std::string& sql, std::string& err) {
    sqlite3* h = dbh(db);
    if (!h) {
        err = "sqlite_exec: database is not open";
        return false;
    }

    char* errmsg = nullptr;
    const int rc = sqlite3_exec(h, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) {
            err = std::string("sqlite_exec rc=") + std::to_string(rc) + ": " + errmsg;
            sqlite3_free(errmsg);
        } else {
            set_err(err, h, rc, "sqlite_exec");
        }
        return false;
    }

    if (errmsg) sqlite3_free(errmsg);
    err.clear();
    return true;
}

bool sqlite_query_cols(
    SqliteDb& db,
    const std::string& sql,
    const std::function<void(const Cols&, const Row&)>& on_row,
    std::string& err
) {
    sqlite3* h = dbh(db);
    if (!h) {
        err = "sqlite_query: database is not open";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const int rc_prep = sqlite3_prepare_v2(h, sql.c_str(), -1, &stmt, nullptr);
    if (rc_prep != SQLITE_OK) {
        set_err(err, h, rc_prep, "sqlite_query prepare");
        if (stmt) sqlite3_finalize(stmt);
        return false;
    }

    const int ncol = sqlite3_column_count(stmt);
    Cols cols;
    cols.reserve(static_cast<size_t>(ncol));
    for (int i = 0; i < ncol; ++i) {
        const char* cn = sqlite3_column_name(stmt, i);
        cols.emplace_back(cn ? cn : "");
    }

    for (;;) {
        const int rc_step = sqlite3_step(stmt);
        if (rc_step == SQLITE_ROW) {
            Row row;
            row.reserve(static_cast<size_t>(ncol));
            for (int i = 0; i < ncol; ++i) {
                // Convert every column to text for now.
                const unsigned char* txt = sqlite3_column_text(stmt, i);
                row.emplace_back(txt ? reinterpret_cast<const char*>(txt) : "");
            }
            on_row(cols, row);
            continue;
        }

        if (rc_step == SQLITE_DONE) {
            break;
        }

        // Error
        set_err(err, h, rc_step, "sqlite_query step");
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    err.clear();
    return true;
}

bool sqlite_query(
    SqliteDb& db,
    const std::string& sql,
    const std::function<void(const Row&)>& on_row,
    std::string& err
) {
    return sqlite_query_cols(db, sql,
        [&](const Cols&, const Row& r) {
            on_row(r);
        },
        err
    );
}

#else

std::string sqlite_version() {
    return "(not linked)";
}

bool sqlite_is_open(const SqliteDb&) noexcept {
    return false;
}

bool sqlite_open(SqliteDb&, const std::string&, std::string& err) {
    err = "SQLite not available (DOTTALK_SQLITE_AVAILABLE=0)";
    return false;
}

void sqlite_close(SqliteDb&) noexcept {
}

bool sqlite_exec(SqliteDb&, const std::string&, std::string& err) {
    err = "SQLite not available (DOTTALK_SQLITE_AVAILABLE=0)";
    return false;
}

bool sqlite_query_cols(SqliteDb&, const std::string&, const std::function<void(const Cols&, const Row&)>&, std::string& err) {
    err = "SQLite not available (DOTTALK_SQLITE_AVAILABLE=0)";
    return false;
}

bool sqlite_query(SqliteDb&, const std::string&, const std::function<void(const Row&)>&, std::string& err) {
    err = "SQLite not available (DOTTALK_SQLITE_AVAILABLE=0)";
    return false;
}

#endif

} // namespace dottalk::sqlite
