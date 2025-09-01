// src/cli/cmd_use.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <fstream>

#include "xbase.hpp"
#include "textio.hpp"
#include "order_state.hpp"
#include "order_hooks.hpp"
#include "cli/memo_auto.hpp"   // <-- memo sidecar binder

namespace fs = std::filesystem;

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
        case 0x83: // dBASE III + memo
        case 0x8B: // dBASE IV + memo
        case 0x8E: // dBASE IV SQL + memo
        case 0xF5: // FoxPro 2.x + memo
        case 0xE5: // Clipper + memo
            return true;
        default:
            return (ver & 0x80) != 0; // many memo variants set the high bit
    }
}

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

    // ---- pre-open: detach any prior memo sidecar in this work area ----
    cli_memo::memo_auto_on_close(a);

    // ---- close any existing order for this area ----
    try { orderstate::clearOrder(a); } catch (...) { /* ignore */ }

    // ---- open table ----
    try {
        a.close();
        a.open(path.string());
        // Ensure DbArea knows its filename (in case open() didn’t set it)
        a.setFilename(path.string());

        std::cout << "Opened " << path.filename().string()
                  << " with " << a.recCount() << " records.\n";
    } catch (const std::exception& e) {
        std::cout << "Open failed: " << e.what() << "\n";
        return;
    }

    // ---- memo sidecar auto-open (bound to this area) ----
    {
        const bool hasMemoFields = dbf_header_indicates_memo(path.string());
        std::string memoErr;
        // Pass the full DBF path so the sidecar lives alongside the table
        if (!cli_memo::memo_auto_on_use(a, path.string(), hasMemoFields, memoErr)) {
            // If STRICT is enabled via set_memo_config, this can be fatal;
            // default behavior is warn and continue.
            std::cout << "USE: " << memoErr << "\n";
        }
    }

    // ---- auto-attach <opened>.inx unless NOINDEX ----
    if (!bypassIndex) {
        fs::path idx = path;           // same directory as the DBF
        idx.replace_extension(".inx");
        try {
            if (fs::exists(idx)) {
                orderstate::setOrder(a, idx.string());
            }
        } catch (...) {
            // ignore filesystem/index attach errors; keep table open
        }
    }
}
