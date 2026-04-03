// src/cli/cmd_turbo_pack.cpp
// TURBOPACK — fast, low-level DBF compaction (byte-oriented)
// Removes physically deleted records (* flag) by rewriting only live ones.
//
// Scope / contract:
//   - Fast path for plain DBF tables only.
//   - Memo tables are explicitly refused.
//   - X64 tables are explicitly refused for now.
//   - Leaves order/index state detached; reindex is recommended afterward.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <ctime>

#include "xbase.hpp"
#include "xbase_64.hpp"
#include "cli/order_state.hpp"

#if __has_include("cli/path_resolver.hpp") && __has_include("cli/cmd_setpath.hpp")
  #include "cli/path_resolver.hpp"
  #include "cli/cmd_setpath.hpp"
  #define HAVE_PATHS 1
#else
  #define HAVE_PATHS 0
#endif

namespace {
    namespace fs = std::filesystem;
    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;

    inline u16 le16(const u8* p) {
        return u16(p[0]) | (u16(p[1]) << 8);
    }

    inline u32 le32(const u8* p) {
        return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24);
    }

    inline void put_le32(u8* p, u32 v) {
        p[0] = u8(v & 0xFF);
        p[1] = u8((v >> 8)  & 0xFF);
        p[2] = u8((v >> 16) & 0xFF);
        p[3] = u8((v >> 24) & 0xFF);
    }

    static std::string s8(const fs::path& p) {
#if defined(_WIN32)
        auto u = p.u8string();
        return std::string(u.begin(), u.end());
#else
        return p.string();
#endif
    }

    static bool ends_with_ci(std::string_view s, std::string_view suf) {
        if (s.size() < suf.size()) return false;
        for (size_t i = 0; i < suf.size(); ++i) {
            if (std::toupper(static_cast<unsigned char>(s[s.size() - suf.size() + i])) !=
                std::toupper(static_cast<unsigned char>(suf[i]))) {
                return false;
            }
        }
        return true;
    }

#if HAVE_PATHS
    namespace paths = dottalk::paths;
    static fs::path dbf_root() {
        try { return paths::get_slot(paths::Slot::DBF); }
        catch (...) { return fs::current_path(); }
    }
#else
    static fs::path dbf_root() { return fs::current_path(); }
#endif

    static fs::path resolve_dbf_slot(const fs::path& p) {
        if (p.is_absolute()) return p;
        return fs::weakly_canonical(dbf_root() / p);
    }

    static fs::path temp_target_for(const fs::path& src) {
        auto dir = src.parent_path();
        auto stem = src.stem().string();
        auto ext = src.extension().string();
        return dir / (stem + ".turbo.tmp" + ext);
    }

    static std::optional<fs::path> derive_current_dbf_path(xbase::DbArea& A) {
        if (!A.filename().empty()) {
            return fs::weakly_canonical(fs::path(A.filename()));
        }

        std::string base = A.dbfBasename();
        if (base.empty()) base = A.logicalName();
        if (base.empty()) return std::nullopt;

        fs::path guess = resolve_dbf_slot(base + ".dbf");
        if (fs::exists(guess) && fs::is_regular_file(guess)) return guess;

        // case-insensitive fallback
        std::string lower = base;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        guess = resolve_dbf_slot(lower + ".dbf");
        if (fs::exists(guess) && fs::is_regular_file(guess)) return guess;

        return std::nullopt;
    }

    static void update_header_timestamp_and_count(std::string& header, u32 new_count) {
        if (header.size() < 32) return;

        std::time_t now = std::time(nullptr);
        std::tm tmv;
    #if defined(_WIN32)
        localtime_s(&tmv, &now);
    #else
        tmv = *std::localtime(&now);
    #endif

        u8 yy = static_cast<u8>(tmv.tm_year % 100);
        u8 mm = static_cast<u8>(tmv.tm_mon + 1);
        u8 dd = static_cast<u8>(tmv.tm_mday);

        header[1] = static_cast<char>(yy);
        header[2] = static_cast<char>(mm);
        header[3] = static_cast<char>(dd);

        put_le32(reinterpret_cast<u8*>(&header[4]), new_count);
    }
}

void cmd_TURBOPACK(xbase::DbArea& A, std::istringstream& /*S*/)
{
    using namespace std;

    if (!A.isOpen()) {
        cout << "TURBOPACK: No table open.\n";
        return;
    }

    // TURBOPACK is intentionally limited to plain DBF tables.
    // Memo tables require sidecar-aware remapping / rebuild logic.
    if (A.memoKind() != xbase::DbArea::MemoKind::NONE) {
        cout << "TURBOPACK: Memo tables not supported. Use PACK instead.\n";
        return;
    }

    // X64 tables carry additional header/state semantics (including a second
    // record count in the extension header). Keep TURBOPACK conservative.
    if (A.versionByte() == xbase::DBF_VERSION_64) {
        cout << "TURBOPACK: X64 tables not supported. Use PACK instead.\n";
        return;
    }

    auto srcOpt = derive_current_dbf_path(A);
    if (!srcOpt) {
        cout << "TURBOPACK: Cannot determine DBF file path.\n";
        return;
    }

    fs::path srcP = *srcOpt;
    string srcStr = s8(srcP);

    if (!ends_with_ci(srcStr, ".dbf")) {
        cout << "TURBOPACK: Not a .dbf file: " << srcStr << "\n";
        return;
    }

    if (!fs::exists(srcP) || !fs::is_regular_file(srcP)) {
        cout << "TURBOPACK: File not found: " << srcStr << "\n";
        return;
    }

    // Remember order for later warning
    string origOrder;
    try { origOrder = orderstate::orderName(A); } catch (...) {}

    // Close & clear order before touching the file
    try { orderstate::clearOrder(A); } catch (...) {}
    try { A.close(); } catch (...) {}

    cout << "TURBOPACK processing: " << srcStr << "\n";

    ifstream in(srcStr, ios::binary);
    if (!in) {
        cout << "TURBOPACK: Cannot open source file.\n";
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    // Read minimal header prefix
    u8 hdr32[32]{};
    in.read(reinterpret_cast<char*>(hdr32), 32);
    if (!in) {
        cout << "TURBOPACK: Cannot read header.\n";
        in.close();
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    u16 headerLen = le16(&hdr32[8]);
    u16 recLen    = le16(&hdr32[10]);
    u32 origCount = le32(&hdr32[4]);

    if (headerLen < 33 || recLen < 1 || headerLen > 4096) {
        cout << "TURBOPACK: Invalid header lengths.\n";
        in.close();
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    // Read full header
    string header(headerLen, '\0');
    in.seekg(0, ios::beg);
    in.read(&header[0], headerLen);
    if (!in || static_cast<unsigned char>(header.back()) != 0x0D) {
        cout << "TURBOPACK: Invalid or incomplete header (missing 0x0D terminator).\n";
        in.close();
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    in.seekg(headerLen, ios::beg);

    fs::path tmpP = temp_target_for(srcP);
    ofstream out(s8(tmpP), ios::binary | ios::trunc);
    if (!out) {
        cout << "TURBOPACK: Cannot create temp file " << s8(tmpP) << "\n";
        in.close();
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    // Write header with count=0 initially + current date
    update_header_timestamp_and_count(header, 0);
    out.write(header.data(), headerLen);
    if (!out) {
        cout << "TURBOPACK: Failed writing header to temp.\n";
        out.close(); in.close();
        fs::remove(tmpP);
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    vector<char> rec(recLen);
    u32 kept = 0;

    while (true) {
        in.read(rec.data(), recLen);
        if (in.eof()) break;
        if (!in) {
            cout << "TURBOPACK: Read error after " << kept << " kept records.\n";
            out.close(); in.close();
            fs::remove(tmpP);
            try { A.open(srcStr); } catch (...) {}
            return;
        }

        if (static_cast<u8>(rec[0]) != xbase::IS_DELETED) {
            rec[0] = xbase::NOT_DELETED;
            out.write(rec.data(), recLen);
            if (!out) {
                cout << "TURBOPACK: Write error after " << kept << " kept records.\n";
                out.close(); in.close();
                fs::remove(tmpP);
                try { A.open(srcStr); } catch (...) {}
                return;
            }
            ++kept;
        }
    }

    in.close();

    // Final header update with correct count + date
    update_header_timestamp_and_count(header, kept);
    out.seekp(0, ios::beg);
    out.write(header.data(), headerLen);
    if (!out) {
        cout << "TURBOPACK: Failed to update header with final count.\n";
        out.close();
        fs::remove(tmpP);
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    // EOF marker
    char eofc = char(0x1A);
    out.write(&eofc, 1);
    out.flush();
    out.close();

    // Truncate to exact size (safety)
    auto expected_size = uint64_t(headerLen) + uint64_t(kept) * recLen + 1;
    error_code ec_trunc;
    fs::resize_file(tmpP, expected_size, ec_trunc);
    // ignore error - non-fatal

    // Atomic replace
    fs::path bakP = srcP;
    bakP.replace_extension(".turbo.bak");

    error_code ec;
    fs::remove(bakP, ec);

    fs::rename(srcP, bakP, ec);
    if (ec) {
        cout << "TURBOPACK: Cannot create backup: " << ec.message() << "\n";
        fs::remove(tmpP);
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    fs::rename(tmpP, srcP, ec);
    if (ec) {
        cout << "TURBOPACK: Cannot replace original file: " << ec.message() << "\n";
        // rollback attempt
        error_code ec2;
        fs::rename(bakP, srcP, ec2);
        if (ec2) {
            cout << "  Rollback also failed. Original may be lost.\n";
        }
        try { A.open(srcStr); } catch (...) {}
        return;
    }

    fs::remove(bakP, ec);  // best effort

    // Reopen
    try {
        A.open(srcStr);
        orderstate::clearOrder(A);

        u32 reported = static_cast<u32>(A.recCount());
        if (reported != kept) {
            cout << "Warning: reopened record count mismatch! Expected " << kept
                 << ", got " << reported << ".\n"
                 << "  Try CLOSE then USE again.\n";
        }

        // Put the cursor at TOP for predictable post-pack behavior.
        if (kept > 0) {
            try { A.top(); } catch (...) {}
        }
    }
    catch (const exception& e) {
        cout << "TURBOPACK: packed file written, but reopen failed: " << e.what() << "\n";
        cout << "  Try manually: USE " << srcP.stem().string() << "\n";
    }
    catch (...) {
        cout << "TURBOPACK: packed file written, but reopen failed.\n";
    }

    cout << "TURBOPACK complete. Kept " << kept << " of " << origCount << " records.\n";
    if (!origOrder.empty()) {
        cout << "Note: previous order '" << origOrder << "' detached. Reindex recommended.\n";
    } else {
        cout << "Note: reindex recommended after TURBOPACK.\n";
    }
}
