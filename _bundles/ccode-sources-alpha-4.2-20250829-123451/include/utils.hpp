#pragma once
#include <string>
#include <cctype>

namespace xbase_internal {
// case-insensitive ends_with; inline for internal TUs
inline bool ends_with_ci(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    size_t off = s.size() - suf.size();
    for (size_t i = 0; i < suf.size(); ++i) {
        unsigned char a = static_cast<unsigned char>(s[off + i]);
        unsigned char b = static_cast<unsigned char>(suf[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}
} // namespace xbase_internal
