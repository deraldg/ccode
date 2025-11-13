// src/cli/index_legacy_loader.cpp
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <sstream>
#include <algorithm>

bool load_inx_recnos(const std::string& path,
                     int32_t /*maxRecno*/,
                     std::vector<uint32_t>& out,
                     std::string* err)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = "open failed"; return false; }

    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)), {});
    out.clear();

    // Try binary: little-endian 32-bit recnos
    if (buf.size() >= 4 && (buf.size() % 4) == 0) {
        out.reserve(buf.size() / 4);
        for (size_t i = 0; i + 3 < buf.size(); i += 4) {
            uint32_t v = (uint32_t)buf[i]
                       | ((uint32_t)buf[i+1] << 8)
                       | ((uint32_t)buf[i+2] << 16)
                       | ((uint32_t)buf[i+3] << 24);
            if (v) out.push_back(v);
        }
        if (!out.empty()) return true;
    }

    // Fallback: ASCII numbers (space/comma separated)
    std::string s(reinterpret_cast<const char*>(buf.data()), buf.size());
    std::replace(s.begin(), s.end(), ',', ' ');
    std::istringstream iss(s);
    unsigned long long n;
    while (iss >> n) {
        if (n > 0 && n <= 0xFFFFFFFFull) out.push_back((uint32_t)n);
    }
    if (!out.empty()) return true;

    if (err) *err = "could not parse recnos";
    return false;
}


