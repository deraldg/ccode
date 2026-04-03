#pragma once
#include <string>
#include <cstdint>

#include "xbase.hpp"
#include "memo_wiring.hpp"
#include "memo_backend.hpp"

namespace memo_display {

// right-trim spaces and CR/LF only (preserve internal whitespace and case)
inline void rtrim_inplace(std::string& s) {
    while (!s.empty()) {
        const char c = s.back();
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') s.pop_back();
        else break;
    }
}

// Render memo preview for the CURRENT record and given field.
// Returns true and fills `out` on success; false leaves `out` untouched.
inline bool render_current(xbase::DbArea& area,
                           const std::string& fieldName,
                           std::string& out)
{
    if (!cli_memo::field_is_memo(area, fieldName)) return false;

    std::string err;
    auto* store = cli_memo::require_memo_backend(area, &err);
    if (!store) return false;

    // Current phase-1 contract:
    // the DBF memo field stores the raw token in the row slot.
    int fld1 = 0;
    {
        const auto defs = area.fields();
        for (std::size_t i = 0; i < defs.size(); ++i) {
            if (defs[i].name == fieldName) {
                fld1 = static_cast<int>(i) + 1;
                break;
            }
        }
    }
    if (fld1 <= 0) return false;

    std::string token;
    try {
        token = area.get(fld1);   // raw memo token stored in DBF field slot
    } catch (...) {
        out.clear();
        return true;
    }

    dottalk::memo::MemoRef ref{token};
    if (store->is_null_ref(ref)) {
        out.clear();
        return true;
    }

    auto gr = store->get_text(ref);
    if (!gr.ok) {
        // DISPLAY should not crash on memo read failure.
        out.clear();
        return true;
    }

    std::string s = gr.text;
    rtrim_inplace(s);
    if (s.size() > 256) s.resize(256);
    out.swap(s);
    return true;
}

} // namespace memo_display