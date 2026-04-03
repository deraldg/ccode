// src/cli/cmd_zap.cpp
// ZAP — removes ALL records from the current DBF, preserving structure (header + field descriptors).
//       Rewrites the file with record count = 0, updated timestamp, and EOF marker.

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/order_state.hpp"

#if __has_include("cli/cmd_setpath.hpp")
  #include "cli/cmd_setpath.hpp"
  #define HAVE_SETPATH 1
#else
  #define HAVE_SETPATH 0
#endif

#if __has_include("cnx/cnx.hpp")
  #include "cnx/cnx.hpp"
  #define HAVE_CNX 1
#else
  #define HAVE_CNX 0
#endif

namespace fs = std::filesystem;

namespace {

static inline fs::path dbf_slot_root_fallback() {
#if HAVE_SETPATH
    try { return dottalk::paths::get_slot(dottalk::paths::Slot::DBF); } catch (...) {}
#endif
    return fs::current_path();
}

static inline fs::path resolve_dbf_slot_path(fs::path p) {
    if (p.empty())                return p;
    if (p.is_absolute())          return p;

    // Try SET PATH slot first
#if HAVE_SETPATH
    try {
        fs::path root = dottalk::paths::get_slot(dottalk::paths::Slot::DBF);
        if (!root.empty() && fs::exists(root)) {
            return fs::absolute(root / p);
        }
    } catch (...) {}
#endif

    // Fallback: absolute from current directory
    try { return fs::absolute(p); } catch (...) { return p; }
}

static inline std::string s8(const fs::path& p) {
#if defined(_WIN32)
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
#else
    return p.string();
#endif
}

static inline std::uint16_t read_le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

static inline void set_dbf_last_updated_and_reccount(
    std::vector<std::uint8_t>& hdrBlock,
    std::uint32_t recCount)
{
    if (hdrBlock.size() < 32) return;

    std::time_t now = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &now);
#else
    tmv = *std::localtime(&now);
#endif

    // dBASE year is year - 1900, usually modulo 100
    const std::uint8_t yy = static_cast<std::uint8_t>(tmv.tm_year % 100);
    const std::uint8_t mm = static_cast<std::uint8_t>(tmv.tm_mon + 1);
    const std::uint8_t dd = static_cast<std::uint8_t>(tmv.tm_mday);

    hdrBlock[1] = yy;
    hdrBlock[2] = mm;
    hdrBlock[3] = dd;

    hdrBlock[4] = static_cast<std::uint8_t>(recCount & 0xFF);
    hdrBlock[5] = static_cast<std::uint8_t>((recCount >> 8)  & 0xFF);
    hdrBlock[6] = static_cast<std::uint8_t>((recCount >> 16) & 0xFF);
    hdrBlock[7] = static_cast<std::uint8_t>((recCount >> 24) & 0xFF);
}

static inline fs::path make_temp_path(const fs::path& orig, const std::string& tag) {
    fs::path dir  = orig.parent_path();
    std::string stem = orig.stem().string();
    std::string ext  = orig.extension().string();
    return dir / (stem + "." + tag + ".tmp" + ext);
}

static inline fs::path make_bak_path(const fs::path& orig, const std::string& tag) {
    fs::path dir  = orig.parent_path();
    std::string stem = orig.stem().string();
    std::string ext  = orig.extension().string();
    return dir / (stem + "." + tag + ".bak" + ext);
}

static bool replace_file_with_backup(const fs::path& orig, const fs::path& tmp, const fs::path& bak) {
    std::error_code ec;

    // Remove stale backup if exists
    if (fs::exists(bak, ec)) {
        fs::remove(bak, ec);
    }

    // original → backup
    fs::rename(orig, bak, ec);
    if (ec) {
        std::cout << "ZAP: Failed to rename original to backup: " << ec.message() << "\n";
        return false;
    }

    // temp → original
    fs::rename(tmp, orig, ec);
    if (ec) {
        std::cout << "ZAP: Failed to replace original: " << ec.message() << "\n";
        // Rollback attempt
        std::error_code ec2;
        fs::rename(bak, orig, ec2);
        if (ec2) {
            std::cout << "  Rollback also failed — manual recovery needed!\n";
        } else {
            std::cout << "  Original restored.\n";
        }
        return false;
    }

    // Cleanup backup (best effort)
    fs::remove(bak, ec);
    return true;
}

static void try_mark_index_dirty(const std::string& orderContainer) {
    if (orderContainer.empty()) return;

    fs::path ip = resolve_dbf_slot_path(fs::path(orderContainer));
    std::string ext = textio::up(ip.extension().string());

#if HAVE_CNX
    if (ext == ".CNX") {
        cnxfile::CNXHandle* h = nullptr;
        if (cnxfile::open(ip.string(), h) && h) {
            cnxfile::set_dirty(h, true);
            cnxfile::close(h);
            std::cout << "ZAP: CNX marked dirty (reindex recommended): " << s8(ip.filename()) << "\n";
            return;
        }
        std::cout << "ZAP: note: CNX found but could not mark dirty\n";
        return;
    }
#endif

    if (ext == ".INX" || ext == ".CDX") {
        std::cout << "ZAP: note: index container should be rebuilt: " << s8(ip.filename()) << "\n";
        return;
    }

    std::cout << "ZAP: note: order container should be rebuilt: " << s8(ip.filename()) << "\n";
}

} // namespace

void cmd_ZAP(xbase::DbArea& A, std::istringstream& in) {
    (void)in;  // ZAP takes no arguments

    if (!A.isOpen() || A.filename().empty()) {
        std::cout << "ZAP: No table open\n";
        return;
    }

    if (A.memoKind() != xbase::DbArea::MemoKind::NONE) {
        std::cout << "ZAP: Cannot zap memo table (memo block handling not implemented).\n";
        return;
    }

    std::string orderContainer;
    try { orderContainer = orderstate::orderName(A); } catch (...) {}

    // Remember path **before** closing
    std::string orig_filename = A.filename();

    // Close and detach order
    try { orderstate::clearOrder(A); } catch (...) {}
    try { A.close(); } catch (...) {}

    fs::path origPath = resolve_dbf_slot_path(fs::path(orig_filename));
    if (!textio::ends_with_ci(s8(origPath), ".dbf")) {
        origPath.replace_extension(".dbf");
    }

    if (!fs::exists(origPath)) {
        std::cout << "ZAP: File not found: " << s8(origPath) << "\n";
        return;
    }

    std::cout << "Zapping: " << s8(origPath) << "\n";

    const fs::path tmpPath = make_temp_path(origPath, "zap");
    const fs::path bakPath = make_bak_path(origPath, "zap");

    // ── Read original header ─────────────────────────────────────────────────
    std::ifstream src(origPath, std::ios::binary);
    if (!src) {
        std::cout << "ZAP: Cannot open file for reading\n";
        return;
    }

    std::uint8_t hdr32[32]{};
    src.read(reinterpret_cast<char*>(hdr32), 32);
    if (!src) {
        std::cout << "ZAP: Failed to read header\n";
        return;
    }

    const std::uint16_t headerLen = read_le16(hdr32 + 8);
    if (headerLen < 33 || headerLen > 4096) {
        std::cout << "ZAP: Invalid header length (" << headerLen << ")\n";
        return;
    }

    std::vector<std::uint8_t> headerBlock(headerLen);
    src.seekg(0, std::ios::beg);
    src.read(reinterpret_cast<char*>(headerBlock.data()), headerLen);
    if (!src) {
        std::cout << "ZAP: Failed to read full header block\n";
        return;
    }

    if (headerBlock.back() != 0x0D) {
        std::cout << "Warning: header does not end with 0x0D terminator\n";
    }

    src.close();

    // ── Create empty table (header + EOF) ────────────────────────────────────
    set_dbf_last_updated_and_reccount(headerBlock, 0);

    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cout << "ZAP: Cannot create temporary file: " << s8(tmpPath) << "\n";
        return;
    }

    out.write(reinterpret_cast<const char*>(headerBlock.data()), headerLen);
    if (!out) {
        std::cout << "ZAP: Failed writing header to temp file\n";
        out.close();
        fs::remove(tmpPath);
        return;
    }

    const char eof_marker = static_cast<char>(0x1A);
    out.write(&eof_marker, 1);
    if (!out) {
        std::cout << "ZAP: Failed writing EOF marker\n";
        out.close();
        fs::remove(tmpPath);
        return;
    }

    out.flush();
    out.close();

    // Force exact size (header + 1 byte EOF)
    std::error_code ec_size;
    fs::resize_file(tmpPath, headerLen + 1, ec_size);
    // ignore error — usually not needed

    // ── Atomic replace ───────────────────────────────────────────────────────
    if (!replace_file_with_backup(origPath, tmpPath, bakPath)) {
        fs::remove(tmpPath);
        return;
    }

    // ── Reopen ───────────────────────────────────────────────────────────────
    try {
        A.open(s8(origPath));
        std::cout << "Reopened successfully. Records: " << A.recCount() << "\n";
    } catch (const std::exception& ex) {
        std::cout << "ZAP: File rewritten (0 records), but reopen failed: " << ex.what() << "\n";
        std::cout << "  Try manually: USE " << origPath.stem().string() << "\n";
        return;
    } catch (...) {
        std::cout << "ZAP: File rewritten (0 records), but reopen failed.\n";
        return;
    }

    // Clear order (indexes invalid)
    try { orderstate::clearOrder(A); } catch (...) {}

    try_mark_index_dirty(orderContainer);

    // Position at beginning (BOF)
    try { A.top(); } catch (...) {}

    std::cout << "ZAP complete. All records removed (0 records remain).\n";
}