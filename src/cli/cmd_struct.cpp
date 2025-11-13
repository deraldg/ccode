// src/cli/cmd_struct.cpp — STRUCT with tag discovery from .inx
#include "xbase.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// If your project already has order_state helpers, include them:
#include "order_state.hpp"

using xbase::DbArea;
namespace fs = std::filesystem;

// -----------------------------
// Minimal 1INX( v1 ) tag reader
// -----------------------------
// Current writer (see cmd_index.cpp) emits:
//   magic:  '1','I','N','X'        (4 bytes)
//   version: uint16_t (1)
//   nameLen: uint16_t
//   name:    bytes[nameLen]   // the tag expression/name (e.g., "LNAME", "sid", or formula)
//   count:   uint32_t         // number of entries
//   ... entries (keyLen(uint16), key bytes, recno(uint32)) ...
//
// We only need the tag names. Return vector for future multi-tag support.
static inline uint16_t rd_u16(std::istream& in) {
    unsigned char b0=0,b1=0;
    if (!in.read(reinterpret_cast<char*>(&b0), 1)) throw std::runtime_error("EOF");
    if (!in.read(reinterpret_cast<char*>(&b1), 1)) throw std::runtime_error("EOF");
    return static_cast<uint16_t>(b0 | (static_cast<uint16_t>(b1) << 8));
}
static inline uint32_t rd_u32(std::istream& in) {
    unsigned char b0=0,b1=0,b2=0,b3=0;
    if (!in.read(reinterpret_cast<char*>(&b0), 1)) throw std::runtime_error("EOF");
    if (!in.read(reinterpret_cast<char*>(&b1), 1)) throw std::runtime_error("EOF");
    if (!in.read(reinterpret_cast<char*>(&b2), 1)) throw std::runtime_error("EOF");
    if (!in.read(reinterpret_cast<char*>(&b3), 1)) throw std::runtime_error("EOF");
    return static_cast<uint32_t>(b0 | (static_cast<uint32_t>(b1) << 8)
                                   | (static_cast<uint32_t>(b2) << 16)
                                   | (static_cast<uint32_t>(b3) << 24));
}

// Try to read a single-tag 1INX v1 file and return the tag name/expression.
// If the file is missing, malformed, or not 1INX, return empty vector.
static std::vector<std::string> read_inx_tags_1inx(const fs::path& p) {
    std::vector<std::string> tags;
    std::ifstream in(p, std::ios::binary);
    if (!in) return tags;

    // Read magic (4 bytes)
    char magic[4] = {0,0,0,0};
    if (!in.read(magic, 4)) return tags;
    if (!(magic[0]=='1' && magic[1]=='I' && magic[2]=='N' && magic[3]=='X')) {
        // Unknown format, bail silently
        return tags;
    }

    // Read version
    uint16_t ver = 0;
    try {
        ver = rd_u16(in);
    } catch (...) { return tags; }

    if (ver != 1) {
        // Unsupported version (future proof: ignore gracefully)
        return tags;
    }

    // Read single tag name length + name
    uint16_t nameLen = 0;
    try {
        nameLen = rd_u16(in);
    } catch (...) { return tags; }

    if (nameLen == 0) return tags;

    std::string name;
    name.resize(nameLen);
    if (!in.read(&name[0], static_cast<std::streamsize>(nameLen))) return tags;

    // We don’t need to read the entries; we already have the expression/name.
    tags.push_back(name);
    return tags;
}

// -------------------------------------
// STRUCT command (prints DBF + index info)
// -------------------------------------
void cmd_STRUCT(DbArea& A, std::istringstream& /*args*/) {
    using std::cout;
    using std::endl;

    if (!A.isOpen()) {
        cout << "No file open in current area.\n";
        return;
    }

    // Header: Datafile path + basename
    const std::string full = A.name();
    fs::path p(full);
    const std::string base = p.filename().string();

    cout << "Fields (" << A.fields().size() << ")\n";
    cout << "  #  Name          Type    Len    Dec\n";
    int i = 1;
    for (const auto& f : A.fields()) {
        // f.name, f.type, f.length, f.decimals (based on your FieldDef)
        cout << "  " << std::setw(2) << i++ << "  "
             << std::left << std::setw(12) << f.name << " "
             << std::left << std::setw(2)  << f.type  << "   "
             << std::setw(6) << f.length   << " "
             << std::setw(5) << f.decimals  << "\n";
    }

    // Index file path (from ORDER state, if present)
    std::string indexPath = "(none)";
    if (orderstate::hasOrder(A)) {
        indexPath = orderstate::orderName(A);
    }

    // Read tag(s) from index file if present
    std::vector<std::string> tags;
    if (indexPath != "(none)") {
        try {
            tags = read_inx_tags_1inx(indexPath);
        } catch (...) {
            // best effort; leave tags empty on parse error
        }
    }

    // Derive Active tag:
    // Prefer a parsed tag name; fallback to ORDER state’s name (path) if no parsed tags.
    std::string activeTag = "(none)";
    if (!tags.empty()) {
        activeTag = tags.front();
    } else if (orderstate::hasOrder(A)) {
        // ORDER name may be a path; show only file stem to avoid confusion
        fs::path ap(orderstate::orderName(A));
        activeTag = ap.stem().string();
        if (activeTag.empty()) activeTag = "(unknown)";
    }

    // Footer block (your requested order):
    // Dbfile, then fields (already printed above), then Index file, then Tags, then Active tag.
    cout << "Dbfile      : " << full << "  (" << base << ")\n";
    cout << "Index file  : " << indexPath << "\n";

    cout << "Tags        : ";
    if (tags.empty()) {
        cout << "(none)\n";
    } else {
        for (size_t k = 0; k < tags.size(); ++k) {
            if (k) cout << ", ";
            cout << tags[k];
        }
        cout << "\n";
    }

    cout << "Active tag  : " << activeTag << "\n";
}
