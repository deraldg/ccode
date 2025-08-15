// src/cli/cmd_list.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"             // xbase::DbArea
#include "cli/settings.hpp"      // cli::Settings (deleted ON/OFF)

namespace {

// Safely print the current record buffer from DbArea.
static void emitRow(std::ostream& out, xbase::DbArea& area) {
    // Show recno, then all fields by index.
    out << "[" << area.recno() << "] ";
    const int n = area.fieldCount();          // assumes DbArea exposes this
    for (int i = 0; i < n; ++i) {
        if (i) out << " | ";
        out << area.get(i);                   // DbArea::get(int) -> std::string
    }
    out << "\n";
}

// Sequential LIST with proper advancement.
// - Starts at current record (doesn't force gotoTop) to respect caller’s position.
// - Honors SET DELETED ON by skipping deleted rows when ON.
static int list_sequential(std::ostream& out, xbase::DbArea& area, std::size_t max_rows = 0) {
    std::size_t shown = 0;
    const bool hideDeleted = cli::Settings::deletedOn();

    // Refresh current buffer; returns whether we're on a valid record.
    bool ok = area.skip(0);

    while (ok) {
        if (!hideDeleted || !area.isDeleted()) {
            emitRow(out, area);
            if (max_rows && ++shown >= max_rows) break;
        }

        // Advance to next physical record; stop when we fall off the end.
        ok = area.skip(1);
    }

    return static_cast<int>(shown);
}

} // namespace

// Shell entry point expected by your dispatcher:
//   void cmd_LIST(DbArea&, std::istringstream&)
// Keeps any tiny FOR/WHILE/etc parsing you may add later; right now it just lists.
void cmd_LIST(xbase::DbArea& area, std::istringstream& args)
{
    // Example: LIST [n]  -> limit rows
    std::size_t limit = 0;
    {
        std::string tok;
        if (args >> tok) {
            // if the first token is a positive integer, treat it as a row limit
            try {
                size_t pos = 0;
                unsigned long v = std::stoul(tok, &pos, 10);
                if (pos == tok.size()) limit = static_cast<std::size_t>(v);
            } catch (...) {
                // ignore non-numeric first token; leave limit = 0 (no limit)
            }
        }
    }

    list_sequential(std::cout, area, limit);
}
