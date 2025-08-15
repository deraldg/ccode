#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>

#include "xbase.hpp"
#include "textio.hpp"

namespace predicates {

// Case-insensitive field lookup; returns 1-based index or 0 if not found.
inline int field_index_ci(const xbase::DbArea& a, const std::string& name) {
    const auto& f = a.fields();
    for (size_t i = 0; i < f.size(); ++i) {
        if (textio::ieq(f[i].name, name)) {
            return static_cast<int>(i + 1); // 1-based
        }
    }
    return 0;
}

// Supported ops: =, ==, !=, <>, >, >=, <, <=, $, $= (contains, case-insensitive)
inline bool eval(const xbase::DbArea& a,
                 const std::string& fld,
                 const std::string& op,
                 const std::string& raw_val)
{
    int idx = field_index_ci(a, fld);
    if (idx <= 0) return false;

    std::string lv = a.get(idx);
    std::string rv = textio::unquote(raw_val);

    // Case-insensitive compare helpers
    auto tolc = [](std::string s){
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return s;
    };

    std::string lv_lc = tolc(lv);
    std::string rv_lc = tolc(rv);

    // Equality / inequality
    if (op == "=" || op == "==") return lv_lc == rv_lc;
    if (op == "!=" || op == "<>") return lv_lc != rv_lc;

    // Contains (FoxPro-style $)
    if (op == "$" || op == "$=") return !rv_lc.empty() && (lv_lc.find(rv_lc) != std::string::npos);

    // Lexicographic comparisons (string)
    if (op == ">")  return lv_lc >  rv_lc;
    if (op == ">=") return lv_lc >= rv_lc;
    if (op == "<")  return lv_lc <  rv_lc;
    if (op == "<=") return lv_lc <= rv_lc;

    // Unknown op => false (caller can warn)
    return false;
}

} // namespace predicates
