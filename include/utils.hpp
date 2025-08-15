#pragma once
#include <string>
#include <cctype>
#include <cstddef> // size_t

namespace xbase_internal {

// Case-insensitive ends_with for ASCII. Kept inline to avoid ODR/linker issues.
inline bool ends_with_ci(const std::string& s, const std::string& suf) noexcept {
    if (s.size() < suf.size()) return false;
    const size_t off = s.size() - suf.size();
    for (size_t i = 0; i < suf.size(); ++i) {
        // Ensure we pass an int in the unsigned char domain to std::tolower
        const unsigned char a_uc = static_cast<unsigned char>(s[off + i]);
        const unsigned char b_uc = static_cast<unsigned char>(suf[i]);
        const int a = std::tolower(static_cast<int>(a_uc));
        const int b = std::tolower(static_cast<int>(b_uc));
        if (a != b) return false;
    }
    return true;
}

} // namespace xbase_internal
