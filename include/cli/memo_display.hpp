#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

#include "cli/memo_wiring.hpp"   // uses field_is_memo(...) and require_memo_store(...)

namespace memo_display {

// right-trim spaces and CR/LF only (preserve internal whitespace and case)
inline void rtrim_inplace(std::string& s) {
    while (!s.empty()) {
        char c = s.back();
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') s.pop_back();
        else break;
    }
}

// Render memo preview for the CURRENT record and given field.
// Returns true and fills `out` on success; false leaves `out` untouched.
inline bool render_current(DbArea& area,
                           const std::string& fieldName,
                           std::string& out)
{
    if (!cli_memo::field_is_memo(area, fieldName)) return false;

    std::string err;
    auto* store = cli_memo::require_memo_store(area, &err);
    if (!store) return false;

    // You may already have a getter for the current record's memo id.
    // Add this to memo_wiring.hpp when you wire your codec:
    //   bool try_get_current_record_memo_id(DbArea&, const std::string&, uint64_t& out_id);
    uint64_t id = 0;
    if (!cli_memo::try_get_current_record_memo_id(area, fieldName, id)) {
        // Not wired yet; treat as empty.
        out.clear();
        return true;
    }
    if (id == 0) { out.clear(); return true; }

    std::vector<uint8_t> bytes;
    try {
        bytes = store->get(id);
    } catch (...) {
        // On read errors, show empty (DISPLAY should not crash)
        out.clear();
        return true;
    }

    std::string s(bytes.begin(), bytes.end()); // preserve case
    rtrim_inplace(s);
    if (s.size() > 256) s.resize(256);
    out.swap(s);
    return true;
}

} // namespace memo_display
