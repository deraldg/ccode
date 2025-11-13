// src/cli/cmd_use.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>   // for std::transform, std::toupper

#include "xbase.hpp"
#include "textio.hpp"
#include "order_state.hpp"
#include "order_hooks.hpp"
#include "cli/memo_auto.hpp"
#include "workareas.hpp"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------
static std::string read_rest_upper_trim(std::istringstream& iss) {
    std::string rest;
    std::getline(iss >> std::ws, rest);
    return textio::up(textio::trim(rest));
}

// Detect if the DBF on disk uses a memo variant by checking header version byte.
static bool dbf_header_indicates_memo(const std::string& dbfPath) {
    std::ifstream f(dbfPath, std::ios::binary);
    if (!f) return false;
    unsigned char ver = 0;
    f.read(reinterpret_cast<char*>(&ver), 1);
    if (!f) return false;

    switch (ver) {
        case 0x83: case 0x8B: case 0x8E: case 0xF5: case 0xE5:
            return true;
        default:
            return (ver & 0x80) != 0;
    }
}

// Case-insensitive uppercase copy
static std::string to_upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

// Simple duplicate-DBF guard: if another area has same filename, close it.
static void close_if_already_open_elsewhere(const std::string& dbfPath) {
    // Normalize to just the base filename, case-insensitive
    const std::string target = to_upper_copy(fs::path(dbfPath).filename().string());
    const size_t n = workareas::count();

    for (size_t i = 0; i < n; ++i) {
        xbase::DbArea* other = workareas::at(i);
        if (!other || !other->isOpen()) continue;

        // DbArea::filename() returns std::string in your tree
        const std::string openedFull = other->filename();
        const std::string openedBase = to_upper_copy(fs::path(openedFull).filename().string());

        if (openedBase == target) {
            // Tidy up order/memo first so metadata doesn’t linger
            try { orderstate::clearOrder(*other); } catch (...) {}
            try { cli_memo::memo_auto_on_close(*other); } catch (...) {}

            // Enforce single-open rule by closing the other area
            try { other->close(); } catch (...) {}

            // Optionally blank the filename so STATUS can't show stale path
            try { other->setFilename(std::string()); } catch (...) {}
        }
    }
}

// ---------------------------------------------------------------------
// USE command
// ---------------------------------------------------------------------
void cmd_USE(xbase::DbArea& a, std::istringstream& iss) {
    // ---- parse required db name ----
    std::string db;
    if (!(iss >> db)) {
        std::cout << "Usage: USE <dbf> [NOINDEX]\n";
        return;
    }

    // ---- parse optional tail (e.g., NOINDEX) ----
    const std::string rest = read_rest_upper_trim(iss);
    const bool bypassIndex = (rest.find("NOINDEX") != std::string::npos);

    // ---- normalize path (.dbf default) ----
    fs::path path = xbase::dbNameWithExt(db);
    if (!fs::exists(path)) {
        std::cout << "Open failed: file not found\n";
        return;
    }

    // ---- pre-open cleanup in this area ----
    cli_memo::memo_auto_on_close(a);
    try { orderstate::clearOrder(a); } catch (...) { /* ignore */ }

    // ---- enforce single-open rule across areas ----
    close_if_already_open_elsewhere(path.string());

    // ---- open table ----
    try {
        a.close();
        a.open(path.string());
        a.setFilename(path.string());

        std::cout << "Opened " << path.filename().string()
                  << " with " << a.recCount() << " records.\n";
    } catch (const std::exception& e) {
        std::cout << "Open failed: " << e.what() << "\n";
        return;
    }

    // ---- memo sidecar auto-open ----
    {
        const bool hasMemoFields = dbf_header_indicates_memo(path.string());
        std::string memoErr;
        if (!cli_memo::memo_auto_on_use(a, path.string(), hasMemoFields, memoErr)) {
            std::cout << "USE: " << memoErr << "\n";
        }
    }

    // ---- auto-attach <opened>.inx unless NOINDEX ----
    if (!bypassIndex) {
        fs::path idx = path;
        idx.replace_extension(".inx");
        try {
            if (fs::exists(idx)) {
                orderstate::setOrder(a, idx.string());
            }
        } catch (...) { /* ignore */ }
    }
}
