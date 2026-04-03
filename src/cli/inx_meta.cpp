// inx_meta.cpp ? DotTalk++ INX metadata reader (implementation)
// See inx_meta.hpp for the on-disk header layout.
//
// This module intentionally uses only the C++ standard library to avoid
// entanglement with writers or xindex internals.

#include "inx_meta.hpp"

#include <fstream>
#include <vector>
#include <array>
#include <cstring>

namespace dottalk {
namespace inx {

namespace {
// Little-endian readers (portable, independent of host endianness)
bool read_exact(std::ifstream& f, void* buf, std::size_t n) {
    return static_cast<bool>(f.read(static_cast<char*>(buf), static_cast<std::streamsize>(n)));
}
bool read_u16_le(std::ifstream& f, std::uint16_t& out) {
    std::array<unsigned char,2> b{};
    if (!read_exact(f, b.data(), b.size())) return false;
    out = static_cast<std::uint16_t>(b[0] | (static_cast<std::uint16_t>(b[1])<<8));
    return true;
}
bool read_u32_le(std::ifstream& f, std::uint32_t& out) {
    std::array<unsigned char,4> b{};
    if (!read_exact(f, b.data(), b.size())) return false;
    out = static_cast<std::uint32_t>(b[0])
        | (static_cast<std::uint32_t>(b[1])<<8)
        | (static_cast<std::uint32_t>(b[2])<<16)
        | (static_cast<std::uint32_t>(b[3])<<24);
    return true;
}
} // namespace

Meta read_inx_header(const std::string& path) {
    Meta m;
    m.path = path;

    std::ifstream f(path, std::ios::binary);
    if (!f) { m.error = "open_failed"; return m; }

    // Read and check magic "1INX"
    char magic[4] = {0,0,0,0};
    if (!read_exact(f, magic, 4)) { m.error = "short_read_magic"; return m; }
    if (!(magic[0]=='1' && magic[1]=='I' && magic[2]=='N' && magic[3]=='X')) {
        m.error = "bad_magic"; return m;
    }

    // Version (u16 LE)
    std::uint16_t ver = 0;
    if (!read_u16_le(f, ver)) { m.error = "short_read_version"; return m; }
    m.version = ver;

    // exprName length (u16 LE)
    std::uint16_t name_len = 0;
    if (!read_u16_le(f, name_len)) { m.error = "short_read_name_len"; return m; }

    // Guard ridiculous lengths (e.g., > 64k is already impossible with u16, but cap tighter)
    if (name_len > 8192) { m.error = "expr_name_len_too_large"; return m; }

    // Read exprName
    std::string expr;
    expr.resize(name_len);
    if (name_len > 0) {
        if (!read_exact(f, expr.data(), name_len)) { m.error = "short_read_name"; return m; }
        // Strip trailing NULs if writer ever padded (be tolerant)
        while (!expr.empty() && expr.back()=='\0') expr.pop_back();
    }
    m.key_expr = expr;

    // entryCount (u32 LE)
    std::uint32_t nentries = 0;
    if (!read_u32_le(f, nentries)) { m.error = "short_read_entry_count"; return m; }
    m.entry_count = nentries;

    m.ok = true;
    return m;
}

} // namespace inx
} // namespace dottalk



