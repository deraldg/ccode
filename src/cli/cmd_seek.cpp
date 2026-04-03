// src/cli/cmd_seek.cpp
//
// SEEK
//
// Dev-tool contract:
// - FoxPro-style: if an active order exists, "SEEK <value>" uses the active tag.
// - If active order is CDX, prefer the area-local IndexManager / CDX backend.
// - Otherwise fall back to sequential scan on the requested field.
// - This file must NOT own raw LMDB state directly.
//
// Supported forms:
//   SEEK <value>
//   SEEK <value> IN <field>
//   SEEK <field> = <value>
//   SEEK <field> <value>
//   SEEK TRACE ON|OFF
//
// Notes:
// - Exact-match semantics only for now.
// - Comparison is case-insensitive trimmed character comparison,
//   matching the previous dev-tool behavior.
// - When routed through CDX, iteration follows the current active direction
//   and returns the first exact record encountered in that order.

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "textio.hpp"
#include "cli/order_state.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

static bool g_seek_trace = false;

struct CursorRestore {
    xbase::DbArea* a{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& area) : a(&area) {
        try {
            saved = area.recno();
            active = (saved >= 1 && saved <= area.recCount());
        } catch (...) {
            active = false;
        }
    }

    ~CursorRestore() {
        if (!active || !a) return;
        try {
            if (a->gotoRec(saved)) {
                (void)a->readCurrent();
            }
        } catch (...) {
            // best-effort restore only
        }
    }

    CursorRestore(const CursorRestore&) = delete;
    CursorRestore& operator=(const CursorRestore&) = delete;
};

static inline std::string trim_copy(const std::string& s) {
    return textio::trim(s);
}

static inline std::string upper_copy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static inline std::string strip_outer_quotes(std::string s) {
    s = trim_copy(s);
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') ||
            (a == '\'' && b == '\'')) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static inline std::string norm_seek_text(std::string s) {
    return upper_copy(trim_copy(s));
}

static inline bool ieq(const std::string& a, const char* b) {
    if (!b) return false;
    size_t m = 0;
    while (b[m] != '\0') ++m;
    if (a.size() != m) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

static int find_field_1based(const xbase::DbArea& a, const std::string& name_ci) {
    const auto& fds = a.fields();
    for (size_t i = 0; i < fds.size(); ++i) {
        if (ieq(fds[i].name, name_ci.c_str())) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

static void seek_usage() {
    std::cout
        << "Usage:\n"
        << "  SEEK <value> IN <field> [TRACE ON|OFF]\n"
        << "  SEEK <field> = <value> [TRACE ON|OFF]\n"
        << "  SEEK <field> <value>   [TRACE ON|OFF]\n"
        << "  SEEK <value>           (uses active order/tag when set)\n"
        << "  SEEK TRACE ON|OFF\n";
}

static bool field_value_matches(xbase::DbArea& a, int fld, const std::string& needleU) {
    const std::string curU = norm_seek_text(a.get(fld));

    if (g_seek_trace) {
        std::cout << "; SEEK TRACE: recno=" << a.recno()
                  << " val='" << curU
                  << "' needle='" << needleU << "'\n";
    }

    return curU == needleU;
}

static bool seek_via_cdx_active_order(xbase::DbArea& a,
                                      int fld,
                                      const std::string& fieldU,
                                      const std::string& needleU,
                                      int& found_recno) {
    found_recno = 0;

    if (!orderstate::hasOrder(a)) return false;
    if (!orderstate::isCdx(a)) return false;

    const std::string path   = orderstate::orderName(a);
    const std::string tagRaw = orderstate::activeTag(a);
    const std::string tagU   = upper_copy(tagRaw);

    if (path.empty() || tagU.empty()) return false;

    // Only use active-order CDX path when SEEK is targeting the active tag.
    if (fieldU != tagU) return false;

    auto& im = a.indexManager();
    std::string err;

    if (!im.hasBackend() || !im.isCdx() || im.containerPath() != path) {
        if (!im.openCdx(path, tagU, &err)) {
            return false;
        }
    } else {
        if (!im.setTag(tagU, &err)) {
            return false;
        }
    }

    if (!im.hasBackend() || !im.isCdx() || im.containerPath() != path || im.activeTag().empty()) {
        return false;
    }

    auto cur = im.scan(xindex::Key{}, xindex::Key{});
    if (!cur) return false;

    xindex::Key k;
    xindex::RecNo r;

    const bool asc = orderstate::isAscending(a);
    bool ok = asc ? cur->first(k, r) : cur->last(k, r);

    while (ok) {
        const int32_t rn = static_cast<int32_t>(r);
        if (rn > 0 && rn <= a.recCount()) {
            try {
                if (a.gotoRec(rn) && a.readCurrent()) {
                    if (a.isDeleted()) {
                        // skip deleted for this dev-tool behavior, same as old fallback
                    } else if (field_value_matches(a, fld, needleU)) {
                        found_recno = rn;
                        return true;
                    }
                }
            } catch (...) {
                // keep scanning
            }
        }

        ok = asc ? cur->next(k, r) : cur->prev(k, r);
    }

    return true; // CDX path was used, but no exact match found
}

static bool seek_via_sequential_scan(xbase::DbArea& a,
                                     int fld,
                                     const std::string& needleU,
                                     int& found_recno) {
    found_recno = 0;

    const int32_t total = a.recCount();
    for (int32_t rn = 1; rn <= total; ++rn) {
        try {
            if (!a.gotoRec(rn) || !a.readCurrent()) continue;
            if (a.isDeleted()) continue;
        } catch (...) {
            continue;
        }

        if (field_value_matches(a, fld, needleU)) {
            found_recno = rn;
            return true;
        }
    }
    return true;
}

} // anonymous namespace

void cmd_SEEK(xbase::DbArea& a, std::istringstream& iss)
{
    if (!a.isOpen()) {
        std::cout << "(empty)\n";
        return;
    }

    CursorRestore restore(a);

    // Allow: SEEK TRACE ON|OFF
    {
        std::string first;
        if (iss >> first) {
            if (ieq(first, "TRACE")) {
                std::string onoff;
                iss >> onoff;
                if (ieq(onoff, "ON")) g_seek_trace = true;
                else if (ieq(onoff, "OFF")) g_seek_trace = false;
                std::cout << "SEEK TRACE is " << (g_seek_trace ? "ON" : "OFF") << ".\n";
                return;
            }
            iss.clear();
            iss.seekg(0);
        }
    }

    // Tokenize remaining args.
    std::vector<std::string> toks;
    for (std::string t; iss >> t; ) {
        toks.push_back(t);
    }

    if (toks.empty()) {
        seek_usage();
        return;
    }

    // Some shells re-inject the command name as the first token.
    if (!toks.empty() && ieq(toks[0], "SEEK")) {
        toks.erase(toks.begin());
        if (toks.empty()) {
            seek_usage();
            return;
        }
    }

    // Optional trailing TRACE ON|OFF
    if (toks.size() >= 2 && ieq(toks[toks.size() - 2], "TRACE")) {
        const std::string onoff = toks.back();
        if (ieq(onoff, "ON")) g_seek_trace = true;
        else if (ieq(onoff, "OFF")) g_seek_trace = false;
        toks.resize(toks.size() - 2);
        if (toks.empty()) {
            seek_usage();
            return;
        }
    }

    std::string field;
    std::string value;

    // FoxPro-style: SEEK <value> uses current order/tag if present.
    if (toks.size() == 1) {
        value = toks[0];
        if (orderstate::hasOrder(a)) {
            field = orderstate::activeTag(a);
        }
        if (field.empty()) {
            seek_usage();
            return;
        }
    } else if (toks.size() >= 3 && ieq(toks[1], "IN")) {
        // SEEK <value> IN <field>
        value = toks[0];
        field = toks[2];
    } else if (toks.size() >= 3 && toks[1] == "=") {
        // SEEK <field> = <value>
        field = toks[0];
        value = toks[2];
    } else if (toks.size() >= 2) {
        // SEEK <field> <value>
        field = toks[0];
        value = toks[1];
    } else {
        seek_usage();
        return;
    }

    if (field.empty() || value.empty()) {
        seek_usage();
        return;
    }

    const int fld = find_field_1based(a, field);
    if (fld <= 0) {
        std::cout << "SEEK: unknown field: " << field << "\n";
        return;
    }

    const std::string fieldU  = upper_copy(field);
    const std::string needleU = norm_seek_text(strip_outer_quotes(value));

    int found_recno = 0;
    bool routed = false;

    // Prefer active CDX order when SEEK targets the active tag.
    if (orderstate::hasOrder(a) && orderstate::isCdx(a)) {
        routed = seek_via_cdx_active_order(a, fld, fieldU, needleU, found_recno);
    }

    // Fallback: sequential scan on field.
    if (!routed) {
        routed = seek_via_sequential_scan(a, fld, needleU, found_recno);
    }

    if (found_recno > 0) {
        try {
            if (a.gotoRec(found_recno) && a.readCurrent()) {
                restore.active = false; // keep cursor on found record
                std::cout << "Found at " << found_recno << ".\n";
                return;
            }
        } catch (...) {
            // fall through
        }
    }

    std::cout << "Not found.\n";
}