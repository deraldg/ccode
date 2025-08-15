#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "xindex/key_common.hpp"


namespace xindex {

// Very simple key encoders with lexicographic ordering.
// These are placeholders you can evolve (bump codec_version in Fingerprint).
namespace codec {

inline std::vector<uint8_t> encodeChar(const std::string& s, size_t width, bool upper = false) {
    std::string tmp = s;
    if (upper) std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    if (tmp.size() < width) tmp.append(width - tmp.size(), ' ');
    else if (tmp.size() > width) tmp.resize(width);
    return std::vector<uint8_t>(tmp.begin(), tmp.end());
}

inline std::vector<uint8_t> encodeNumber(double x, int width = 18, int decimals = 6) {
    // Fixed-width, zero-padded string; simplistic
    char buf[64]; std::snprintf(buf, sizeof(buf), "%0*.*f", width, decimals, x);
    return std::vector<uint8_t>(buf, buf + std::strlen(buf));
}

inline std::vector<uint8_t> encodeDateYYYYMMDD(const std::string& yyyymmdd) {
    // Trust input shape for now
    return std::vector<uint8_t>(yyyymmdd.begin(), yyyymmdd.end());
}

inline void concat(std::vector<uint8_t>& dst, const std::vector<uint8_t>& more) {
    dst.insert(dst.end(), more.begin(), more.end());
}

} // namespace codec

} // namespace xindex
