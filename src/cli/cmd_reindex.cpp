// src/cli/cmd_reindex.cpp — REINDEX [<tagfile>]
//
// Rebuilds a 1INX index file *without* depending on orderstate::* or IndexManager
// rebuild APIs. It reuses the same shape as your writer: collect (key, recno),
// sort, then write "1INX" (version=1) with the tag expression stored in the file.
//
// Usage:
//   REINDEX
//     -> Rebuilds "<table-base>.inx" next to the currently open DBF, e.g. students.dbf -> students.inx
//   REINDEX <path-or-name>
//     -> Rebuilds that specific .inx (absolute/relative; adds .inx if missing)
//
// Notes:
//   - We read the existing .inx header to recover the tag expression token (e.g. LNAME or #2).
//   - We do *not* try to auto-attach or fiddle with ORDER state; this strictly regenerates the file.
//   - If the inferred/explicit .inx doesn’t exist, we tell the user to create it via INDEX ON … TAG …
//
// Dependencies: standard C++ + xbase::DbArea

#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using xbase::DbArea;

// ----------------------------- helpers ---------------------------------------

static std::string trim_copy(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Canonicalize name: strip spaces/tabs/NUL, keep [A-Za-z0-9_], uppercase.
static std::string canon(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\0' || s[b] == '\t')) ++b;
    while (e > b && (s[e-1] == ' ' || s[e-1] == '\0' || s[e-1] == '\t')) --e;

    std::string out;
    out.reserve(e - b);
    for (size_t i = b; i < e; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isalnum(c) || c == '_')
            out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out;
}

static bool is_hashnum(const std::string& s, int& n_out) {
    if (s.size() >= 2 && s[0] == '#') {
        std::string num = s.substr(1);
        if (!num.empty() && std::all_of(num.begin(), num.end(),
                [](unsigned char c){ return std::isdigit(c) != 0; })) {
            n_out = std::stoi(num);
            return true;
        }
    }
    return false;
}

// write little-endian u16/u32
static void wr_u16(std::ofstream& out, uint16_t v) {
    unsigned char b[2] = {
        static_cast<unsigned char>( v        & 0xFF),
        static_cast<unsigned char>((v >> 8)  & 0xFF)
    };
    out.write(reinterpret_cast<const char*>(b), 2);
}
static void wr_u32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = {
        static_cast<unsigned char>( v        & 0xFF),
        static_cast<unsigned char>((v >> 8)  & 0xFF),
        static_cast<unsigned char>((v >> 16) & 0xFF),
        static_cast<unsigned char>((v >> 24) & 0xFF)
    };
    out.write(reinterpret_cast<const char*>(b), 4);
}

// read little-endian u16/u32
static bool rd_u16(std::ifstream& in, uint16_t& v) {
    unsigned char b[2];
    if (!in.read(reinterpret_cast<char*>(b), 2)) return false;
    v = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
    return true;
}
static bool rd_u32(std::ifstream& in, uint32_t& v) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    v = static_cast<uint32_t>(b[0] |
        (static_cast<uint32_t>(b[1]) << 8) |
        (static_cast<uint32_t>(b[2]) << 16) |
        (static_cast<uint32_t>(b[3]) << 24));
    return true;
}

// Extract the expression token stored in a "1INX" file written by your indexer.
static bool read_inx_expr(const fs::path& p, std::string& exprTokOut, uint16_t& verOut) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;

    char magic[4] = {};
    if (!in.read(magic, 4)) return false;
    if (!(magic[0]=='1' && magic[1]=='I' && magic[2]=='N' && magic[3]=='X')) return false;

    uint16_t ver = 0, nameLen = 0;
    if (!rd_u16(in, ver)) return false;
    if (!rd_u16(in, nameLen)) return false;

    std::string name;
    name.resize(nameLen);
    if (!in.read(name.data(), static_cast<std::streamsize>(nameLen))) return false;

    exprTokOut = name;
    verOut = ver;
    return true;
}

// Build (rewrite) the index file for the given expression token.
// The token is either a field name (case-insensitive) or "#n" (1-based field index).
static bool build_inx(DbArea& A, const std::string& exprTok, const fs::path& outPath) {
    if (!A.isOpen()) { std::cout << "REINDEX: no table open.\n"; return false; }

    const auto& Fs = A.fields();
    const int fcount = static_cast<int>(Fs.size());

    int fldIdx = -1;
    int hashN = 0;
    if (is_hashnum(exprTok, hashN) && hashN >= 1 && hashN <= fcount) {
        fldIdx = hashN;
    } else {
        const std::string want = canon(exprTok);
        for (int i = 0; i < fcount; ++i) {
            if (canon(Fs[static_cast<size_t>(i)].name) == want) { fldIdx = i + 1; break; }
        }
        if (fldIdx < 1) {
            std::cout << "REINDEX: unknown field token '" << exprTok << "'.\n";
            return false;
        }
    }

    struct Entry { std::string key; uint32_t recno; };
    std::vector<Entry> ents;
    ents.reserve(static_cast<size_t>(std::max(0, A.recCount())));

    const int32_t total = A.recCount();
    for (int32_t rn = 1; rn <= total; ++rn) {
        if (!A.gotoRec(rn)) continue;
        if (!A.readCurrent()) continue;
        if (A.isDeleted()) continue;
        std::string k = A.get(fldIdx);
        ents.push_back(Entry{ std::move(k), static_cast<uint32_t>(rn) });
    }

    std::sort(ents.begin(), ents.end(),
        [](const Entry& a, const Entry& b){
            if (a.key == b.key) return a.recno < b.recno;
            return a.key < b.key;
        });

    fs::path out = outPath;
    if (!out.has_extension()) out.replace_extension(".inx");

    std::ofstream outFile(out, std::ios::binary | std::ios::trunc);
    if (!outFile) {
        std::cout << "REINDEX: cannot write file: " << out.string() << "\n";
        return false;
    }

    // Write "1INX" header (same layout as your index writer)
    outFile.write("1INX", 4);
    wr_u16(outFile, 1); // version
    wr_u16(outFile, static_cast<uint16_t>(exprTok.size()));
    outFile.write(exprTok.data(), static_cast<std::streamsize>(exprTok.size()));
    wr_u32(outFile, static_cast<uint32_t>(ents.size()));
    for (const auto& e : ents) {
        uint16_t klen = static_cast<uint16_t>(std::min<size_t>(e.key.size(), 0xFFFFu));
        wr_u16(outFile, klen);
        outFile.write(e.key.data(), klen);
        wr_u32(outFile, e.recno);
    }
    outFile.flush();

    std::cout << "REINDEX: wrote " << out.filename().string()
              << "  (tag expr: " << exprTok << ", ASC)\n";
    return true;
}

// ------------------------------- command -------------------------------------

// REINDEX [<tagfile>]
// - If <tagfile> provided, use it (adds .inx if missing).
// - Else derive "<table-base>.inx" next to the open DBF and use that.
// - Reads the tag expression from the file, then rebuilds.
void cmd_REINDEX(DbArea& A, std::istringstream& args) {
    if (!A.isOpen()) { std::cout << "No table open.\n"; return; }

    std::string argRest;
    std::getline(args, argRest);
    argRest = trim_copy(argRest);

    fs::path tagPath;
    if (!argRest.empty()) {
        tagPath = fs::path(argRest);
    } else {
        // Derive from current table path: <table>.dbf -> <table>.inx
        fs::path dbPath(A.name()); // A.name() shows full or relative table path
        if (dbPath.empty()) {
            std::cout << "REINDEX: cannot infer tag path (unknown table path). "
                         "Specify a tag file: REINDEX <tagfile.inx>\n";
            return;
        }
        tagPath = dbPath;
        tagPath.replace_extension(".inx");
    }

    if (!tagPath.has_extension()) tagPath.replace_extension(".inx");

    if (!fs::exists(tagPath)) {
        std::cout << "REINDEX: tag file not found: " << tagPath.string() << "\n";
        std::cout << "Hint: create it with: INDEX ON <field|#n> TAG "
                  << tagPath.filename().string() << "\n";
        return;
    }

    std::string exprTok;
    uint16_t ver = 0;
    if (!read_inx_expr(tagPath, exprTok, ver)) {
        std::cout << "REINDEX: cannot read/parse 1INX header in " << tagPath.string() << "\n";
        return;
    }

    std::cout << "REINDEX (rebuild)\n";
    std::cout << "  Index file   : " << tagPath.string() << "\n";
    std::cout << "  Tag expr     : " << exprTok << "  (1INX v" << ver << ")\n";

    if (!build_inx(A, exprTok, tagPath)) {
        std::cout << "REINDEX: failed.\n";
        return;
    }

    std::cout << "Note: Index file was regenerated from its stored tag expression.\n";
    std::cout << "      (Index file path ≠ tag expression. "
                 "File is the on-disk index; tag expr defines its key.)\n";
}
