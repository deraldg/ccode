// src/cli/cmd_pack.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <system_error>
#include <fstream>
#include <cstdint>

#include "xbase.hpp"
#include "order_state.hpp"

using namespace xbase;
namespace fs = std::filesystem;

// --- safe file replace (portable, Windows-friendly) ---
static bool safe_replace_file(const fs::path& src_tmp, const fs::path& dst, std::string& why) {
    std::error_code ec;
    fs::copy_file(src_tmp, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) { why = "copy_file: " + ec.message(); return false; }
    fs::remove(src_tmp, ec); // best-effort
    return true;
}

// --- low-level pack: copy non-deleted records and update header count ---
static bool write_packed(const std::string& inPath, const std::string& outPath, std::string& why) {
    std::ifstream in(inPath, std::ios::binary);
    if (!in) { why = "open input"; return false; }

    HeaderRec hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) { why = "read header"; return false; }

    // Read the whole header block (header+field descriptors up to data_start)
    const std::size_t header_size = static_cast<std::size_t>(hdr.data_start);
    std::vector<char> headerBlock(header_size);
    in.seekg(0, std::ios::beg);
    in.read(headerBlock.data(), static_cast<std::streamsize>(headerBlock.size()));
    if (!in) { why = "read header block"; return false; }

    // Compute physical record count from file size
    in.seekg(0, std::ios::end);
    const std::uint64_t fsize = static_cast<std::uint64_t>(in.tellg());
    if (fsize < header_size) { why = "file too small"; return false; }

    const std::uint64_t record_area = fsize - header_size;
    const std::size_t  rec_len = static_cast<std::size_t>(hdr.cpr);
    if (rec_len == 0) { why = "record length zero"; return false; }

    const std::uint64_t recs_total = record_area / rec_len;

    // First pass: count kept records
    in.seekg(static_cast<std::streamoff>(header_size), std::ios::beg);
    std::vector<char> rec(rec_len);
    std::uint64_t kept = 0;
    for (std::uint64_t i = 0; i < recs_total; ++i) {
        in.read(rec.data(), static_cast<std::streamsize>(rec.size()));
        if (!in) { why = "read record during count"; return false; }
        if (!rec.empty() && rec[0] != 0x2A) ++kept; // not deleted if first byte != '*'
    }

    // Patch headerBlock's record count at standard dBASE offset (4..7) little-endian.
    // (HeaderRec likely has the same layout; this avoids relying on member naming.)
    {
        const std::uint32_t new_count = static_cast<std::uint32_t>(kept);
        if (headerBlock.size() < 8) { why = "header too small"; return false; }
        headerBlock[4] = static_cast<char>( new_count        & 0xFF);
        headerBlock[5] = static_cast<char>((new_count >> 8)  & 0xFF);
        headerBlock[6] = static_cast<char>((new_count >> 16) & 0xFF);
        headerBlock[7] = static_cast<char>((new_count >> 24) & 0xFF);
    }

    // Write out: header, then kept records
    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) { why = "open output"; return false; }
    out.write(headerBlock.data(), static_cast<std::streamsize>(headerBlock.size()));
    if (!out) { why = "write header"; return false; }

    // Second pass: copy non-deleted records
    in.clear();
    in.seekg(static_cast<std::streamoff>(header_size), std::ios::beg);
    for (std::uint64_t i = 0; i < recs_total; ++i) {
        in.read(rec.data(), static_cast<std::streamsize>(rec.size()));
        if (!in) { why = "read record during copy"; return false; }
        if (!rec.empty() && rec[0] != 0x2A) {
            out.write(rec.data(), static_cast<std::streamsize>(rec.size()));
            if (!out) { why = "write record"; return false; }
        }
    }
    out.flush();
    if (!out) { why = "flush output"; return false; }

    return true;
}

void cmd_PACK(DbArea& a, [[maybe_unused]] std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    // Detach any active order (indexes become stale after pack)
    try { orderstate::clearOrder(a); } catch (...) {}

    const fs::path orig = a.name();
    fs::path tmp = orig; tmp += ".pack_tmp";

    // 1) Write compacted file to tmp
    std::string why;
    if (!write_packed(orig.string(), tmp.string(), why)) {
        std::cout << "PACK: failed to write temp (" << why << ")\n";
        return;
    }

    // 2) Close table before replacing file to free handles on Windows
    try { a.close(); } catch (...) {}

    // 3) Replace original with tmp
    if (!safe_replace_file(tmp, orig, why)) {
        std::cout << "PACK: failed to replace original (" << why
                  << "). Temporary file left at: " << tmp.filename().string() << "\n";
        return;
    }

    // 4) Reopen; leave order cleared (stale)
    try {
        a.open(orig.string());
        orderstate::clearOrder(a);
        std::cout << "PACK complete. Reopened " << orig.filename().string()
                  << " with " << a.recCount() << " records.\n";
        std::cout << "(Note: indexes are now stale; rebuild and SET INDEX.)\n";
    } catch (const std::exception& e) {
        std::cout << "PACK: replaced file but reopen failed: " << e.what() << "\n";
    }
}
