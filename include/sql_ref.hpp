// sql_ref.hpp
#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <set>
#include <algorithm>

namespace sqlref {

struct Item {
    const char* name;       // e.g. "CREATE-TABLE", "CREATE-INDEX"
    const char* syntax;     // main syntax line
    const char* summary;    // short description
    const char* category;   // grouping: "DDL", "DML", "Indexing", etc.
    bool portable;          // true = mostly same in SQLite + MSSQL
};

inline const std::vector<Item>& catalog() {
    static const std::vector<Item> items = {
        // ────────────────────────────────────────────────
        // DDL - Data Definition Language
        // ────────────────────────────────────────────────
        {"CREATE-TABLE", 
         "CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, email TEXT UNIQUE);", 
         "Create table with columns, constraints, primary key", "DDL", true},
        {"CREATE-TABLE-MSSQL", 
         "CREATE TABLE users (id INT IDENTITY(1,1) PRIMARY KEY, name NVARCHAR(100) NOT NULL, email NVARCHAR(255) UNIQUE);", 
         "MSSQL version using IDENTITY", "DDL", false},
        {"CREATE-INDEX", 
         "CREATE INDEX idx_users_email ON users (email);", 
         "Single-column index for faster lookups", "Indexing", true},
        {"CREATE-UNIQUE-INDEX", 
         "CREATE UNIQUE INDEX idx_users_email ON users (email);", 
         "Unique index – prevents duplicates", "Indexing", true},
        {"CREATE-COMPOSITE-INDEX", 
         "CREATE INDEX idx_orders_user_date ON orders (user_id, order_date);", 
         "Multi-column index (WHERE + ORDER BY)", "Indexing", true},
        {"DROP-TABLE", 
         "DROP TABLE IF EXISTS users;", 
         "Delete table (IF EXISTS is safe)", "DDL", true},
        {"DROP-INDEX", 
         "DROP INDEX idx_users_email;", 
         "Remove index", "Indexing", true},
        {"ALTER-TABLE-ADD", 
         "ALTER TABLE users ADD COLUMN status TEXT DEFAULT 'active';", 
         "Add new column (SQLite ALTER is limited)", "DDL", false},

        // ────────────────────────────────────────────────
        // DML - Data Manipulation Language
        // ────────────────────────────────────────────────
        {"INSERT", 
         "INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com');", 
         "Insert single row", "DML", true},
        {"INSERT-MULTI", 
         "INSERT INTO users (name, email) VALUES ('Bob','bob@ex.com'), ('Charlie','charlie@ex.com');", 
         "Insert multiple rows", "DML", true},
        {"UPDATE", 
         "UPDATE users SET status = 'active' WHERE id = 5;", 
         "Update matching rows", "DML", true},
        {"DELETE", 
         "DELETE FROM users WHERE id = 42;", 
         "Delete matching rows", "DML", true},
        {"TRUNCATE", 
         "TRUNCATE TABLE users;", 
         "Remove all rows quickly (MSSQL only)", "DML", false},

        // ────────────────────────────────────────────────
        // Querying & Filtering
        // ────────────────────────────────────────────────
        {"SELECT-BASIC", 
         "SELECT name, email FROM users WHERE age > 30 ORDER BY name LIMIT 10;", 
         "Basic query with WHERE, ORDER, LIMIT", "Query", true},
        {"SELECT-COUNT", 
         "SELECT COUNT(*) FROM users WHERE status = 'active';", 
         "Count matching rows", "Query", true},
        {"SELECT-GROUP", 
         "SELECT department, AVG(salary) FROM employees GROUP BY department HAVING COUNT(*) > 5;", 
         "Aggregate + GROUP BY + HAVING", "Query", true},
        {"SELECT-JOIN-INNER", 
         "SELECT u.name, o.product FROM users u INNER JOIN orders o ON u.id = o.user_id;", 
         "Matching rows from both tables", "Join", true},
        {"SELECT-JOIN-LEFT", 
         "SELECT u.name, o.product FROM users u LEFT JOIN orders o ON u.id = o.user_id;", 
         "All left rows + matching right rows", "Join", true},

        // ────────────────────────────────────────────────
        // Indexing & Optimization
        // ────────────────────────────────────────────────
        {"EXPLAIN-QUERY", 
         "EXPLAIN QUERY PLAN SELECT * FROM users WHERE email = 'alice@example.com';", 
         "Show SQLite query plan / index usage", "Optimization", false},
        {"ANALYZE", 
         "ANALYZE users;", 
         "Update statistics for better query planning", "Optimization", true},
        {"VACUUM", 
         "VACUUM;", 
         "Reclaim space after deletes (SQLite)", "Optimization", false},
        {"REINDEX", 
         "REINDEX idx_users_email;", 
         "Rebuild index after heavy updates", "Indexing", true},

        // ────────────────────────────────────────────────
        // Transactions & Safety
        // ────────────────────────────────────────────────
        {"BEGIN-TRAN", 
         "BEGIN TRANSACTION;", 
         "Start transaction", "Transaction", true},
        {"COMMIT", 
         "COMMIT;", 
         "Save changes", "Transaction", true},
        {"ROLLBACK", 
         "ROLLBACK;", 
         "Undo changes", "Transaction", true},

        // ────────────────────────────────────────────────
        // Date & String Functions
        // ────────────────────────────────────────────────
        {"DATE-NOW-SQLITE", 
         "SELECT DATE('now'), TIME('now'), DATETIME('now');", 
         "Current date/time (SQLite)", "Date", false},
        {"DATE-NOW-MSSQL", 
         "SELECT GETDATE();", 
         "Current date/time (MSSQL)", "Date", false},
        {"DATE-ADD-SQLITE", 
         "SELECT DATE('now', '+7 days');", 
         "Add days (SQLite)", "Date", false},
        {"DATE-ADD-MSSQL", 
         "SELECT DATEADD(DAY, 7, GETDATE());", 
         "Add days (MSSQL)", "Date", false},
        {"STRFTIME", 
         "SELECT STRFTIME('%Y-%m-%d', created_at) FROM logs;", 
         "Format date (SQLite)", "Date", false},
        {"FORMAT-MSSQL", 
         "SELECT FORMAT(created_at, 'yyyy-MM-dd') FROM logs;", 
         "Format date (MSSQL)", "Date", false},

        // ────────────────────────────────────────────────
        // Utility / Admin
        // ────────────────────────────────────────────────
        {"PRAGMA-SQLITE", 
         "PRAGMA table_info(users);", 
         "Show table structure (SQLite)", "Utility", false},
        {"SP-HELP-MSSQL", 
         "EXEC sp_help 'users';", 
         "Show table info (MSSQL)", "Utility", false},
    };
    return items;
}

// ────────────────────────────────────────────────
// Lookup helpers
// ────────────────────────────────────────────────

inline const Item* find(std::string_view name_upper) {
    for (const auto& item : catalog()) {
        if (std::string_view(item.name) == name_upper) return &item;
    }
    return nullptr;
}

inline std::vector<const Item*> search(std::string_view token_upper) {
    std::vector<const Item*> matches;
    std::string token(token_upper);
    std::transform(token.begin(), token.end(), token.begin(), ::toupper);

    for (const auto& item : catalog()) {
        std::string n(item.name);
        std::transform(n.begin(), n.end(), n.begin(), ::toupper);

        std::string cat = item.category ? item.category : "";
        std::transform(cat.begin(), cat.end(), cat.begin(), ::toupper);

        if (n.find(token) != std::string::npos ||
            (!cat.empty() && cat.find(token) != std::string::npos)) {
            matches.push_back(&item);
        }
    }
    return matches;
}

inline std::vector<std::string> categories() {
    std::vector<std::string> cats;
    std::set<std::string> seen;
    for (const auto& item : catalog()) {
        if (item.category && seen.insert(item.category).second) {
            cats.push_back(item.category);
        }
    }
    return cats;
}

} // namespace sqlref