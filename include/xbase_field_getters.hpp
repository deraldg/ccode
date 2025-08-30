#pragma once
#include <string>
#include <optional>
#include <cctype>
#include <stdexcept>
#include "xbase.hpp"
#include "textio.hpp"

// Put everything in a tiny namespace to avoid collisions with local helpers.
namespace xfg {

// Resolve field name (case-insensitive, trims) -> 0-based index
inline int resolve_field_index_std(xbase::DbArea& db, const std::string& nameIn) {
    const std::string name = textio::trim(nameIn);
    const auto F = db.fields();
    for (int i = 0; i < static_cast<int>(F.size()); ++i) {
        if (textio::ieq(textio::trim(F[i].name), name)) return i;
    }
    return -1;
}

// Get field as string (right-trim DBF padding)
inline std::string getFieldAsString(xbase::DbArea& db, const std::string& name) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return std::string{};
    std::string s = db.get(idx0 + 1); // your API is 1-based for get()
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

// Get field as number (throws on bad numeric; glue catches and returns nullopt)
inline double getFieldAsNumber(xbase::DbArea& db, const std::string& name) {
    std::string s = getFieldAsString(db, name);
    // left-trim
    size_t i = 0; while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i) s.erase(0, i);
    size_t pos = 0;
    double v = std::stod(s, &pos);
    if (pos != s.size()) throw std::runtime_error("trailing");
    return v;
}

} // namespace xfg
