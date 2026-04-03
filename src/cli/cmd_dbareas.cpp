// src/cli/cmd_dbareas.cpp
//
// DBAREAS - Convenience wrapper around DBAREA.
//
// Key rule (matches cmd_schemas.cpp): filename() is the source of truth for
// whether a slot is "open". DbArea::isOpen() is not reliable during refactors.

#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "cmd_dbarea.hpp"
#include "workareas.hpp"

#if __has_include("set_relations.hpp")
  #include "set_relations.hpp"
  #define HAVE_RELATIONS 1
#else
  #define HAVE_RELATIONS 0
#endif

namespace {

// ---- small helpers ---------------------------------------------------------

static inline std::string trim_copy(std::string s) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [&](unsigned char c){ return !is_space(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [&](unsigned char c){ return !is_space(c); }).base(), s.end());
    return s;
}

static inline std::string up_copy(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static inline bool try_parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

// "Open truth" (matches cmd_schemas.cpp):
// A slot is open if (and only if) filename() is non-empty.
static inline bool open_truth(const xbase::DbArea& a) {
    try {
        return !a.filename().empty();
    } catch (...) {
        return false;
    }
}

static std::string infer_parent_from_area(const xbase::DbArea& a) {
    try {
        const std::string ln = a.logicalName();
        if (!ln.empty()) return up_copy(ln);
        return up_copy(a.name());
    } catch (...) {
        return {};
    }
}

#if HAVE_RELATIONS
static void print_relations_for_current_area(xbase::DbArea& current) {
    // Parent anchor inferred from *current* area identity (logicalName preferred).
    const std::string parent = infer_parent_from_area(current);

    // Wire parent + refresh counts for this anchor.
    try { relations_api::set_current_parent_name(parent); } catch (...) {}
    try { relations_api::refresh_for_current_parent(); } catch (...) {}

    std::cout << "\n";
    std::cout << "Relations\n";
    std::cout << "---------\n";
    std::cout << "Parent anchor        : " << (parent.empty() ? "(unknown)" : parent) << "\n";

    // Direct children + match counts (current parent row context).
    std::vector<std::string> kids;
    try { kids = relations_api::child_areas_for_current_parent(); } catch (...) { kids.clear(); }

    if (kids.empty()) {
        std::cout << "Children             : (none configured)\n";
    } else {
        std::cout << "Children (direct)\n";
        for (const auto& child : kids) {
            int mc = 0;
            try { mc = relations_api::match_count_for_child(child); } catch (...) { mc = 0; }
            std::cout << "  -> " << child << "  (matches: " << mc << ")\n";
        }
    }

    // Tree view (REL LIST ALL style)
    std::cout << "\n";
    std::cout << "Relation tree\n";
    std::cout << "-------------\n";

    std::vector<relations_api::PreviewRow> rows;
    try { rows = relations_api::list_tree_for_current_parent(/*recursive=*/true, /*max_depth=*/64); }
    catch (...) { rows.clear(); }

    if (rows.empty()) {
        std::cout << (parent.empty() ? "(none)" : parent) << "\n";
        return;
    }

    for (const auto& row : rows) {
        // IMPORTANT: PreviewRow field is `line` (not `text`).
        std::cout << row.line << "\n";
    }
}
#endif

} // namespace

// ---- Command ---------------------------------------------------------------

void cmd_DBAREAS(xbase::DbArea& current, std::istringstream& in)
{
    // Consume the remainder as a simple token stream.
    std::string tok;
    if (!(in >> tok)) {
        // No args: behave like DBAREA (current slot only)
        std::istringstream empty;
        cmd_DBAREA(current, empty);
        return;
    }

    const std::string up = up_copy(tok);

    if (up == "ALL") {
        // Print ONLY open slots (filename() truth), using DBAREA for canonical formatting.
        bool any = false;

        for (int i = 0; i < xbase::MAX_AREA; ++i) {
            xbase::DbArea* a = nullptr;
            try { a = workareas::at(i); } catch (...) { a = nullptr; }
            if (!a) continue;
            if (!open_truth(*a)) continue;

            any = true;
            std::istringstream empty;
            cmd_DBAREA(*a, empty);
        }

        if (!any) {
            std::cout << "DBAREAS: no open work areas.\n";
        }
        return;
    }

    if (up == "REL") {
        // Current area summary + relations block (optional)
        std::istringstream empty;
        cmd_DBAREA(current, empty);

#if HAVE_RELATIONS
        print_relations_for_current_area(current);
#else
        std::cout << "\nRelations: (module not present)\n";
#endif
        return;
    }

    // Numeric slot? (0-based)
    int slot = -1;
    if (try_parse_int(tok, slot)) {
        if (slot < 0 || slot >= xbase::MAX_AREA) {
            std::cout << "DBAREAS: slot out of range: " << slot
                      << " (0.." << (xbase::MAX_AREA - 1) << ")\n";
            return;
        }

        xbase::DbArea* a = nullptr;
        try { a = workareas::at(slot); } catch (...) { a = nullptr; }

        if (!a || !open_truth(*a)) {
            std::cout << "DBAREAS: area " << slot << " is not open.\n";
            return;
        }

        std::istringstream empty;
        cmd_DBAREA(*a, empty);
        return;
    }

    std::cout << "Usage:\n";
    std::cout << "  DBAREAS                (same as DBAREA for current)\n";
    std::cout << "  DBAREAS <n>            (print area n)\n";
    std::cout << "  DBAREAS ALL            (print all OPEN areas; filename() is truth)\n";
    std::cout << "  DBAREAS REL            (current area + relations summary/tree)\n";
}
