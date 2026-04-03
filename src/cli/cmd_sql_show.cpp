
// src/cli/cmd_sql_show.cpp
// SHOW family: SHOW TABLE | SHOW COLUMNS | SHOW INDEX
//
// - SHOW TABLE   : one-screen summary of current work area
// - SHOW COLUMNS : column layout (similar to FIELDS/STRUCT but compact)
// - SHOW INDEX   : order/index summary (graceful on non-order builds)

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <filesystem>

#include "xbase.hpp"
#include "xbase_field_getters.hpp"
#include "textio.hpp"
#include "cli/order_state.hpp"

using xbase::DbArea;
namespace fs = std::filesystem;

static inline std::string basename_only(std::string p) {
    // normalize slashes and pull basename
    for (auto &c : p) if (c == '\\') c = '/';
    auto pos = p.find_last_of('/');
    if (pos != std::string::npos) p.erase(0, pos + 1);
    return p;
}

static void show_table(DbArea& A)
{
    if (!A.isOpen()) { std::cout << "No file open\n"; return; }

    // Try to get absolute path, fall back to A.name()
    std::string path = A.name();
    try {
        path = fs::absolute(path).string();
    } catch (...) {
        // leave as-is
    }

    // Deleted count (quick scan, position-preserving)
    int64_t deleted = 0;
    long save_rec = A.recno();
    bool have = false;
    try {
        if (A.top() && A.readCurrent()) {
            do {
                if (A.isDeleted()) ++deleted;
            } while (A.skip(+1) && A.readCurrent());
        }
        have = true;
    } catch (...) {
        // ignore
    }
    // restore position if we moved
    if (have) {
        try { if (save_rec > 0) { A.gotoRec(save_rec); A.readCurrent(); } } catch (...) {}
    }

    std::cout << "Table: " << basename_only(A.name()) << "\n";
    std::cout << "Path : "  << path << "\n";

    // We don't have the engine here to print Area #; RECNO/RECS is enough.
    std::cout << "Recs : " << A.recCount()
              << "   Recno: " << A.recno() << "\n";
    std::cout << "Del  : " << deleted << "\n";

    try {
        if (!orderstate::hasOrder(A)) {
            std::cout << "Order: PHYSICAL\n";
            std::cout << "Index: (none)\n";
            std::cout << "Tag  : (none)\n";
        } else {
            const bool asc = orderstate::isAscending(A);
            const std::string idx = orderstate::orderName(A);
            const std::string tag = orderstate::activeTag(A);
            std::cout << "Order: " << (asc ? "ASCEND" : "DESCEND") << "\n";
            std::cout << "Index: " << (idx.empty() ? std::string("(unknown)") : idx) << "\n";
            std::cout << "Tag  : " << (tag.empty() ? std::string("(none)") : tag) << "\n";
        }
    } catch (...) {
        std::cout << "Order: (unavailable)\n";
        std::cout << "Index: (unavailable)\n";
        std::cout << "Tag  : (unavailable)\n";
    }

    // Optional: Filter flag ? print OFF unless a flag is exposed later.
    std::cout << "Filter: OFF\n";
}

static void show_columns(DbArea& A)
{
    if (!A.isOpen()) { std::cout << "No file open\n"; return; }

    const int n = (int)A.fields().size();
    std::cout << "Field  " << std::left << std::setw(14) << "Name"
              << "Type " << "Len " << "Dec" << "\n";

    for (int i = 0; i < n; ++i) {
        const auto& f = A.fields()[size_t(i)];
        std::cout << std::right << std::setw(5) << (i+1) << "  "
                  << std::left  << std::setw(14) << f.name
                  << f.type << "    "
                  << std::right << std::setw(0) << f.length << "   "
                  << f.decimals << "\n";
    }
}

static void show_index(DbArea& A)
{
    std::cout << "INDEX INFO:\n";
    try {
        if (!A.isOpen()) { std::cout << "  (No file open)\n"; return; }
        if (!orderstate::hasOrder(A)) {
            std::cout << "  Order      : PHYSICAL\n";
            std::cout << "  Index file : (none)\n";
            std::cout << "  Active tag : (none)\n";
            return;
        }
        const bool asc = orderstate::isAscending(A);
        const std::string idx = orderstate::orderName(A);
        const std::string tag = orderstate::activeTag(A);
        std::cout << "  Order      : " << (asc ? "ASCEND" : "DESCEND") << "\n";
        std::cout << "  Index file : " << (idx.empty() ? std::string("(unknown)") : idx) << "\n";
        std::cout << "  Active tag : " << (tag.empty() ? std::string("(none)") : tag) << "\n";
    } catch (...) {
        std::cout << "  (Order API not wired in this build target)\n";
    }
}

void cmd_SQL_SHOW(DbArea& A, std::istringstream& S)
{
    std::string what;
    std::getline(S, what);
    what = textio::trim(what);
    const std::string U = textio::up(what);

    if (U == "TABLE") { show_table(A); return; }
    if (U == "COLUMNS" || U.empty()) { show_columns(A); return; }
    if (U == "INDEX") { show_index(A); return; }

    std::cout << "SHOW syntax\n"
              << "  SHOW TABLE\n"
              << "  SHOW COLUMNS\n"
              << "  SHOW INDEX\n";
}



