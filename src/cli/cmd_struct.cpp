// src/cli/cmd_struct.cpp
// STRUCT / STRUCT INDEX: prints DBF fields + index container info.
// Supports:
//   STRUCT                -> current area (fields + index info)
//   STRUCT INDEX          -> same (explicit index mode)
//   STRUCT ALL            -> all open areas
//   STRUCT ALL INDEX      -> all open areas + index info
//   STRUCT ALL VERBOSE    -> adds CNX tag table where available

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/order_state.hpp"
#include "cnx/cnx.hpp"  // CNX API

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using xbase::DbArea;
namespace fs = std::filesystem;

// Provided by src/cli/shell.cpp
extern "C" xbase::XBaseEngine* shell_engine();

// --- Minimal helpers for 1INX (single-tag) ---
static inline uint16_t rd_u16(std::istream& in) {
    unsigned char b0 = 0, b1 = 0;
    if (!in.read(reinterpret_cast<char*>(&b0), 1)) throw std::runtime_error("EOF");
    if (!in.read(reinterpret_cast<char*>(&b1), 1)) throw std::runtime_error("EOF");
    return static_cast<uint16_t>(b0 | (static_cast<uint16_t>(b1) << 8));
}

static std::vector<std::string> read_inx_tags_1inx(const fs::path& p) {
    std::vector<std::string> tags;
    std::ifstream in(p, std::ios::binary);
    if (!in) return tags;

    char magic[4] = {0, 0, 0, 0};
    if (!in.read(magic, 4)) return tags;
    if (!(magic[0] == '1' && magic[1] == 'I' && magic[2] == 'N' && magic[3] == 'X')) {
        return tags;
    }

    uint16_t ver = 0;
    try {
        ver = rd_u16(in);
    } catch (...) {
        return tags;
    }
    if (ver != 1) return tags;

    uint16_t nameLen = 0;
    try {
        nameLen = rd_u16(in);
    } catch (...) {
        return tags;
    }
    if (nameLen == 0) return tags;

    std::string name(nameLen, '\0');
    if (!in.read(&name[0], static_cast<std::streamsize>(nameLen))) return tags;
    tags.push_back(name);
    return tags;
}

// --- CNX helpers ---
static bool ends_with_ext_ci(const std::string& path, const char* ext3) {
    const size_t n = path.size();
    return n >= 4 && path[n - 4] == '.' &&
           (char)std::toupper((unsigned char)path[n - 3]) == ext3[0] &&
           (char)std::toupper((unsigned char)path[n - 2]) == ext3[1] &&
           (char)std::toupper((unsigned char)path[n - 1]) == ext3[2];
}

static bool read_cnx_tags(const std::string& cnxpath,
                          std::vector<cnxfile::TagInfo>& out) {
    cnxfile::CNXHandle* h = nullptr;
    if (!cnxfile::open(cnxpath, h)) return false;
    bool ok = cnxfile::read_tagdir(h, out);
    cnxfile::close(h);
    return ok;
}

static std::string to_lower(std::string s) {
    for (auto& c : s) {
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}

// ---- core printer for a single area ----
static void print_struct_for_area(DbArea& A, int area_no, bool wantIndex, bool verbose) {
    using std::cout;

    if (!A.isOpen()) return;

    const std::string full = A.name();
    const std::string base = fs::path(full).filename().string();
    const auto& fields = A.fields();
    const std::size_t field_count = fields.size();

    // Header line for multi-area mode
    if (area_no >= 0) {
        cout << "Area " << area_no << ": " << full << "  (" << base << ")\n";
    }

    // Fields header
    cout << "Fields (" << field_count << ")\n";
    cout << "  #  " << std::left << std::setw(12) << "Name"
         << "  "  << std::left << std::setw(4)  << "Type"
         << "  "  << std::right << std::setw(4) << "Len"
         << "  "  << std::right << std::setw(4) << "Dec"
         << "\n";

    // Fields rows ? no blank lines between entries
    int i = 1;
    for (const auto& f : fields) {
        cout << "  " << std::setw(2) << i++ << "  "
             << std::left  << std::setw(12) << f.name << "  "
             << std::left  << std::setw(4)  << f.type << "  "
             << std::right << std::setw(4)  << (int)f.length << "  "
             << std::right << std::setw(4)  << (int)f.decimals
             << "\n";
    }

    // If caller only wants field layout, stop here
    if (!wantIndex) {
        cout << "Dbfile      : " << full << "  (" << base << ")\n";
        return;
    }

    // Resolve order container path from order_state
    std::string indexPath = "(none)";
    if (orderstate::hasOrder(A)) {
        indexPath = orderstate::orderName(A);
    }

    // Collect tags for display
    std::vector<std::string> tags_to_print;
    std::vector<cnxfile::TagInfo> cnx_tags; // keep for verbose table
    std::vector<std::string> inx_tags;      // keep to fallback active tag

    if (indexPath != "(none)") {
        try {
            if (ends_with_ext_ci(indexPath, "CNX")) {
                if (read_cnx_tags(indexPath, cnx_tags)) {
                    tags_to_print.reserve(cnx_tags.size());
                    for (const auto& e : cnx_tags) {
                        tags_to_print.push_back(e.name);
                    }
                }
            } else if (ends_with_ext_ci(indexPath, "INX")) {
                inx_tags = read_inx_tags_1inx(indexPath);
                tags_to_print = inx_tags;
            }
        } catch (...) {
            // best-effort only; no hard failure on bad index file
        }
    }

    // Determine active tag to print:
    // 1) use runtime orderstate::activeTag(A)
    // 2) if empty and container is 1INX with a single tag, use that tag name
    std::string activeTag = orderstate::activeTag(A);
    if (activeTag.empty() && !inx_tags.empty()) {
        activeTag = inx_tags.front();
    }

    // Footer (index-aware)
    cout << "Dbfile      : " << full << "  (" << base << ")\n";
    cout << "Index file  : " << indexPath << "\n";

    cout << "Tags        : ";
    if (tags_to_print.empty()) {
        cout << "(none)\n";
    } else {
        for (std::size_t k = 0; k < tags_to_print.size(); ++k) {
            if (k) cout << ", ";
            cout << tags_to_print[k];
        }
        cout << "\n";
    }

    cout << "Active tag  : "
         << (activeTag.empty() ? std::string("(none)") : activeTag)
         << "\n";

    // Optional verbose CNX table
    if (verbose && !cnx_tags.empty()) {
        cout << "CNX Tags (verbose)\n";
        cout << "  * marks active\n";
        cout << "  " << std::left << std::setw(1)  << " "
             << std::left << std::setw(16) << "Tag"
             << "Expression\n";
        for (const auto& t : cnx_tags) {
            const bool isActive = (!activeTag.empty() && t.name == activeTag);
            std::string expr;
            // NOTE: populate expr from TagInfo fields when available.
            if (expr.empty()) expr = "(expr unavailable)";
            cout << "  " << (isActive ? "*" : " ")
                 << std::left << std::setw(16) << t.name
                 << expr << "\n";
        }
    }
}

// ---- command entry ----
void cmd_STRUCT(DbArea& A, std::istringstream& args) {
    using std::cout;
    using std::string;

    // Collect remaining tokens to allow flexible ordering:
    // e.g., "INDEX ALL", "ALL INDEX", "ALL", "ALL VERBOSE", etc.
    std::vector<string> toks;
    {
        string t;
        while (args >> t) {
            toks.push_back(to_lower(t));
        }
    }

    bool wantIndex = true;   // default: show index info
    bool verbose   = false;
    bool wantAll   = false;

    for (const auto& t : toks) {
        if (t == "index")      wantIndex = true;   // explicit (already default)
        else if (t == "verbose") verbose = true;
        else if (t == "all")   wantAll = true;
        else if (t == "fields") wantIndex = false; // potential alias
        // ignore unknowns to preserve old behavior
    }

    if (wantAll) {
        auto* e = shell_engine();
        if (!e) {
            cout << "No engine available.\n";
            return;
        }

        bool printed_any = false;
        for (int i = 0; i < xbase::MAX_AREA; ++i) {
            DbArea& area = e->area(i);
            if (!area.isOpen()) continue;

            if (printed_any) cout << "\n"; // spacer between areas (not fields)
            print_struct_for_area(area, i, wantIndex, verbose);
            printed_any = true;
        }

        if (!printed_any) {
            cout << "No open areas.\n";
        }
        return;
    }

    // Single-area (current) behavior
    if (!A.isOpen()) {
        cout << "No file open in current area.\n";
        return;
    }

    print_struct_for_area(A, -1, wantIndex, verbose);
}



