#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace cli_planA {

inline std::string hex_dump(const std::vector<char>& bytes, std::size_t max_bytes = 64) {
    std::ostringstream oss;
    std::size_t n = bytes.size();
    if (n > max_bytes) n = max_bytes;
    for (std::size_t i = 0; i < n; ++i) {
        if (i && (i % 16 == 0)) oss << "\n";
        unsigned v = static_cast<unsigned char>(bytes[i]);
        oss << std::hex << std::setw(2) << std::setfill('0') << v << ' ';
    }
    return oss.str();
}

} // namespace cli_planA
