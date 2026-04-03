// cmd_sql_help.cpp
#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "sql_ref.hpp"

#if __has_include("dli/registry.hpp")
  #include "dli/registry.hpp"
  #define DT_HAVE_DLI_REGISTRY 1
#else
  #define DT_HAVE_DLI_REGISTRY 0
#endif

namespace {
std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

void print_item(const sqlref::Item& it, bool verbose = true) {
    std::cout << it.name << "\n";
    std::cout << "  " << it.syntax << "\n";
    std::cout << "  " << it.summary << "\n";
    if (!it.portable) {
        std::cout << "  (SQLite/MSSQL differences apply)\n";
    }
    if (verbose) std::cout << "\n";
}
} // namespace

void show_sql_help(const std::string& arg) {
    std::string term = to_upper(arg);

    if (term.empty()) {
        std::cout << "SQL REFERENCE (SQLite + MSSQL)\n\n"
                  << "Common commands for database work (grouped)\n\n";

        std::map<std::string, std::vector<const sqlref::Item*>> grouped;
        for (const auto& item : sqlref::catalog()) {
            std::string cat = item.category ? item.category : "Uncategorized";
            grouped[cat].push_back(&item);
        }

        for (const auto& [cat, items] : grouped) {
            std::cout << "=== " << cat << " ===\n";
            for (const auto* it : items) {
                std::cout << std::left << std::setw(28) << it->name << it->summary << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "Usage:\n"
                  << "  SQL                        → this grouped list\n"
                  << "  SQL INDEXING               → only indexing commands\n"
                  << "  SQL CREATE-INDEX           → show details\n"
                  << "  SQL LIST-CATEGORIES        → show category names\n"
                  << "  HELP SQL <term>            → same as SQL <term>\n\n"
                  << "Tip: Use EXPLAIN QUERY PLAN (SQLite) or SET SHOWPLAN_ALL ON (MSSQL) to verify indexes.\n";
        return;
    }

    if (term == "LIST" || term == "LIST-CATEGORIES") {
        std::cout << "SQL Categories:\n\n";
        auto cats = sqlref::categories();
        for (const auto& cat : cats) {
            std::cout << "  " << cat << "\n";
        }
        std::cout << "\nExample: HELP SQL INDEXING\n";
        return;
    }

    // Group filter (e.g. HELP SQL INDEXING)
    bool is_group = false;
    std::string group_upper = term;
    for (const auto& item : sqlref::catalog()) {
        if (item.category && to_upper(item.category).find(group_upper) != std::string::npos) {
            if (!is_group) {
                std::cout << "=== " << item.category << " ===\n\n";
                is_group = true;
            }
            std::cout << std::left << std::setw(28) << item.name << item.summary << "\n";
            if (!item.portable) {
                std::cout << "  (SQLite/MSSQL differences apply)\n";
            }
        }
    }
    if (is_group) return;

    // Exact match
    if (const auto* item = sqlref::find(term)) {
        print_item(*item, true);
        return;
    }

    // Partial / contains search
    auto matches = sqlref::search(term);
    if (!matches.empty()) {
        std::cout << "Matching SQL helpers:\n\n";
        for (const auto* m : matches) {
            print_item(*m, false);
            std::cout << "\n";
        }
        return;
    }

    std::cout << "No match for: " << term << "\n"
              << "Try: SQL, HELP SQL LIST-CATEGORIES, or HELP SQL <category>\n";
}

// ────────────────────────────────────────────────
// Command handler + optional registration
// ────────────────────────────────────────────────

// IMPORTANT: Do NOT define cmd_SQL here; cmd_sql.cpp already defines it.
void cmd_SQLHELP(xbase::DbArea& /*area*/, std::istringstream& iss) {
    std::string args;
    std::getline(iss >> std::ws, args);
    show_sql_help(args);
}

#if DT_HAVE_DLI_REGISTRY
// Register only SQLHELP to avoid LNK2005 with cmd_sql.obj.
static bool s_sqlhelp_reg = []() {
    dli::registry().add("SQLHELP", &cmd_SQLHELP);
    return true;
}();
#endif
