// src/cli/cmd_status.cpp — Walk areas and show DBF + order/index info.
// Default: CURRENT area only. `STATUS ALL` or `STATUS OPEN` list open areas;
// add `--verbose|-v` to include closed. `STATUS HELP` prints usage.
//
// "Closed" means: there is no DBF file open in the area.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <cctype>

#include "xbase.hpp"
#include "order_state.hpp"
#include "workareas.hpp"

using xbase::DbArea;

static inline std::string trim_copy(std::string s) {
    auto is_space = [](unsigned char ch){ return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c){ return !is_space(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c){ return !is_space(c); }).base(), s.end());
    return s;
}

static inline bool ends_with_icase(const std::string& s, const char* ext) {
    if (s.size() < std::strlen(ext)) return false;
    std::string tail = s.substr(s.size() - std::strlen(ext));
    std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c){ return std::tolower(c); });
    std::string e(ext);
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return std::tolower(c); });
    return tail == e;
}

static inline bool equals_icase(std::string s, const char* other) {
    if (!other) return false;
    std::string o(other);
    auto to_lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    std::transform(s.begin(), s.end(), s.begin(), to_lower);
    std::transform(o.begin(), o.end(), o.begin(), to_lower);
    return s == o;
}

static inline void header(std::size_t slot, const char* label, bool current) {
    std::cout << "\n-- Area " << slot;
    if (label && *label) std::cout << " [" << label << "]";
    if (current) std::cout << " [current]";
    std::cout << " --\n";
}
static inline void kv(const char* k, const std::string& v) {
    std::cout << "  " << std::left << std::setw(13) << k << ": " << v << "\n";
}

/* Strict “DBF is actually open” probe.
   Why: Some engines report isOpen() when a handle exists but no table is loaded. */
static bool has_dbf_open(DbArea* a) {
    if (!a) return false;

    bool engOpen = false;
    try { engOpen = a->isOpen(); } catch (...) { engOpen = false; }
    if (!engOpen) return false;

    std::string file;
    try { file = a->filename(); } catch (...) {}
    file = trim_copy(file);

    // Heuristic placeholders sometimes returned by engines.
    auto is_placeholder = [](const std::string& s) {
        if (s.empty()) return true;
        std::string t; t.resize(s.size());
        std::transform(s.begin(), s.end(), t.begin(), [](unsigned char c){ return std::tolower(c); });
        return (t == "?" || t == "(nil)" || t == "(none)" || t == "(n/a)");
    };

    if (is_placeholder(file)) {
        // Try secondary identity as a last resort
        try { file = trim_copy(a->name()); } catch (...) {}
        if (is_placeholder(file)) file.clear();
    }

    // Hard probe: table metadata should be readable.
    bool meta_ok = false;
    try { meta_ok = (a->recLength() > 0); } catch (...) { meta_ok = false; }
    if (!meta_ok) {
        try { (void)a->recCount(); meta_ok = true; } catch (...) {}
    }

    // If neither metadata nor filename looks right, treat as closed.
    if (!meta_ok && (file.empty() || !ends_with_icase(file, ".dbf"))) return false;

    return true;
}

static void print_area(DbArea& A)
{
    // Workspace
    std::cout << "Workspace\n";
    std::string file;
    try { file = A.filename(); } catch (...) {}
    if (file.empty()) { try { file = A.name(); } catch (...) {} }
    kv("File", file);
    std::cout << "\n";

    // Table
    std::cout << "Table\n";
    int recs = 0, recno = -1, reclen = 0;
    try { recs  = static_cast<int>(A.recCount()); } catch (...) {}
    try { recno = static_cast<int>(A.recno());    } catch (...) {}
    try { reclen = A.recLength();                 } catch (...) {}
    kv("Records",   std::to_string(recs));
    kv("Recno",     (recno >= 0 ? std::to_string(recno) : std::string("(n/a)")));
    kv("Bytes/rec", (reclen > 0 ? std::to_string(reclen) : std::string("")));
    std::cout << "\n";

    // Order / Index
    std::cout << "Order / Index\n";
    bool asc = true; std::string orderSpec;
    try { asc = orderstate::isAscending(A); } catch (...) {}
    try { orderSpec = orderstate::hasOrder(A) ? orderstate::orderName(A) : std::string(); } catch (...) {}
    kv("Direction",  asc ? "ASCEND" : "DESCEND");

    std::string indexFile;
    std::string activeTag;

    if (!orderSpec.empty()) {
        if (ends_with_icase(orderSpec, ".inx") ||
            ends_with_icase(orderSpec, ".cdx") ||
            ends_with_icase(orderSpec, ".cnx") ||
            orderSpec.find(':')!=std::string::npos ||
            orderSpec.find('\\')!=std::string::npos ||
            orderSpec.find('/')!=std::string::npos) {
            indexFile = orderSpec;
        } else {
            activeTag = orderSpec;
        }
    }

    kv("Index file", indexFile.empty() ? std::string("(none)") : indexFile);
    kv("Active tag", activeTag.empty() ? std::string("(none)") : activeTag);
}

static void print_open_area(std::size_t slot, bool isCurrent) {
    DbArea* a = workareas::at(slot);
    const char* label = workareas::name(slot);
    header(slot, label, isCurrent);
    kv("Open", "YES");
    print_area(*a);
}
static void print_closed_area(std::size_t slot, bool isCurrent) {
    const char* label = workareas::name(slot);
    header(slot, label, isCurrent);
    kv("Open", "NO");
}

static void print_status_help() {
    std::cout <<
R"(Usage:
  STATUS                 Show the current work area (default).
  STATUS CURRENT         Same as default; show only the current area.
  STATUS ALL             List all open areas.
  STATUS OPEN            Alias of ALL (open areas only).
  STATUS ALL -v          List all areas, including closed.
  STATUS ALL --verbose   Same as -v.

Notes:
  - "Closed" means no DBF file is open in that area.
  - The "[current]" marker denotes the engine's current area.
  - Labels come from workareas::name(slot) when available.

Examples:
  STATUS
  STATUS CURRENT
  STATUS ALL
  STATUS OPEN
  STATUS ALL -v
)";
}

extern "C" xbase::XBaseEngine* shell_engine();

void cmd_STATUS(DbArea& /*unused*/, std::istringstream& args)
{
    const std::size_t n = (shell_engine() ? static_cast<std::size_t>(xbase::MAX_AREA) : 0);
    if (n == 0) { std::cout << "No work areas are configured.\n"; return; }

    // Default: CURRENT. `ALL` and `OPEN` are equivalent open-only views; `-v/--verbose` includes closed too.
    std::string tok;
    bool verbose = false;
    enum class Mode { CURRENT, ALL };
    Mode mode = Mode::CURRENT;

    std::vector<std::string> tokens;
    while (args >> tok) tokens.push_back(tok);

    for (const auto& t : tokens) {
        if (equals_icase(t, "HELP") || equals_icase(t, "-h") || equals_icase(t, "--help")) {
            print_status_help();
            return;
        } else if (equals_icase(t, "ALL") || equals_icase(t, "OPEN")) {
            mode = Mode::ALL;
        } else if (equals_icase(t, "CURRENT")) {
            mode = Mode::CURRENT;
        } else if (equals_icase(t, "--verbose") || equals_icase(t, "-v")) {
            verbose = true;
        } else {
            // ignore unknown tokens
        }
    }

    const auto cur = static_cast<std::size_t>(shell_engine()->currentArea());
    if (cur >= n) { std::cout << "Current work area is out of range.\n"; return; }

    if (mode == Mode::CURRENT) {
        DbArea* a = workareas::at(cur);
        if (!has_dbf_open(a)) {
            std::cout << "Current work area " << cur << " is closed.\n";
            return;
        }
        print_open_area(cur, /*isCurrent=*/true);
        return;
    }

    // Mode::ALL (incl. OPEN alias)
    std::size_t open_count = 0;
    for (std::size_t slot = 0; slot < n; ++slot) {
        DbArea* a = workareas::at(slot);
        const bool current = (slot == cur);
        if (has_dbf_open(a)) {
            print_open_area(slot, current);
            ++open_count;
        } else if (verbose) {
            print_closed_area(slot, current);
        }
    }
    if (open_count == 0) {
        if (verbose) std::cout << "No open work areas (all listed as closed).\n";
        else         std::cout << "No open work areas.\n";
    }
}
