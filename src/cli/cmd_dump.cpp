// src/cli/cmd_dump.cpp
// DotTalk++ — DUMP
// Legacy, path-blind record dumper.
// Operates ONLY on the current work area.
// No path resolution, no file opens, no side effects.

#include "xbase.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <cstdint>
#include <limits>

using namespace xbase;

void cmd_DUMP(DbArea& a, std::istringstream&)
{
    // Must have an open table
    if (!a.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    const auto& fields = a.fields();
    const size_t nrecs = a.recCount();

	// DbArea::gotoRec() is 32-bit in this codebase. Guard the cast.
	if (nrecs > static_cast<size_t>(std::numeric_limits<std::int32_t>::max())) {
	    std::cout << "DUMP: record count too large for 32-bit recno (" << nrecs << ").\n";
	    return;
	}

	// Record-number–based iteration (DbArea has no eof())
	for (std::int32_t r = 1; r <= static_cast<std::int32_t>(nrecs); ++r) {
	    a.gotoRec(r);
        a.readCurrent();   // loads internal record buffer

        // Deleted marker (legacy style)
        if (a.isDeleted())
            std::cout << "* ";
        else
            std::cout << "  ";

        // Dump fields, pipe-delimited
        for (size_t i = 0; i < fields.size(); ++i) {
            std::string val;
            try {
                // DbArea uses 1-based field indexing
                val = a.get((int)i + 1);
            } catch (...) {
                val.clear();
            }

            std::cout << val;
            if (i + 1 < fields.size())
                std::cout << " | ";
        }

        std::cout << "\n";
    }
}
