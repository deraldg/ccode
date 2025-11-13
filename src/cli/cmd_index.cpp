// src/cli/cmd_index.cpp
#include "xbase.hpp"
#include "textio.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using xbase::DbArea;

// Canonicalize: trim outer spaces/NULs/tabs, keep [A-Z0-9_], uppercase
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
        // ignore other padding/garbage chars
    }
    return out;
}

static bool is_hashnum(const std::string& s, int& n_out) {
    if (s.size() >= 2 && s[0] == '#') {
        std::string num = s.substr(1);
        if (!num.empty() && std::all_of(num.begin(), num.end(),
            [](unsigned char c){ return std::isdigit(c)!=0; })) {
            n_out = std::stoi(num);
            return true;
        }
    }
    return false;
}

// Write little-endian u16/u32
static void wr_u16(std::ofstream& out, uint16_t v) {
    unsigned char b[2] = {
        static_cast<unsigned char>(v & 0xFF),
        static_cast<unsigned char>((v >> 8) & 0xFF)
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

// Parse: INDEX ON <field|#n> TAG <filename>
static bool parse_args(std::istringstream& in, std::string& field, std::string& tag) {
    std::string onTok;
    if (!(in >> onTok)) return false;
    if (canon(onTok) != "ON") return false;

    if (!(in >> field) || field.empty()) return false;

    std::string tagTok;
    if (!(in >> tagTok)) return false;
    if (canon(tagTok) != "TAG") return false;

    if (!(in >> tag) || tag.empty()) return false;
    return true;
}

// INDEX ON <field|#n> TAG <name>
// Writes <name>.inx in "1INX" format (readable by LIST).
void cmd_INDEX(DbArea& A, std::istringstream& in)
{
    if (!A.isOpen()) { std::cout << "No table open.\n"; return; }

    std::string fieldTok, tag;
    if (!parse_args(in, fieldTok, tag)) {
        std::cout << "Usage: INDEX ON <field|#n> TAG <name>\n";
        std::cout << "Tip: INDEX ON LAST_NAME TAG N\n";
        return;
    }

    const auto& Fs = A.fields();
    const int fcount = static_cast<int>(Fs.size());

    // Resolve field index (1-based for A.get)
    int fldIdx = -1;

    // Allow numeric form "#n"
    int hashN = 0;
    if (is_hashnum(fieldTok, hashN) && hashN >= 1 && hashN <= fcount) {
        fldIdx = hashN;
    } else {
        const std::string want = canon(fieldTok);
        for (int i = 0; i < fcount; ++i) {
            if (canon(Fs[static_cast<size_t>(i)].name) == want) { fldIdx = i + 1; break; }
        }
        if (fldIdx < 1) {
            std::cout << "INDEX: unknown field '" << fieldTok << "'.\n";
            std::cout << "Available:\n";
            for (int i = 0; i < fcount; ++i)
                std::cout << "  " << Fs[static_cast<size_t>(i)].name << "\n";
            std::cout << "Debug canon:\n";
            std::cout << "  want=" << want << "\n";
            for (int i = 0; i < fcount; ++i) {
                std::cout << "  [" << (i+1) << "] canon(" << Fs[static_cast<size_t>(i)].name
                          << ")=" << canon(Fs[static_cast<size_t>(i)].name) << "\n";
            }
            std::cout << "Tip: INDEX ON #3 TAG N   (uses field number)\n";
            return;
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

    fs::path outPath = tag;
    if (!outPath.has_extension()) outPath.replace_extension(".inx");

    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) { std::cout << "INDEX: cannot write file: " << outPath.string() << "\n"; return; }

    // "1INX" format:
    // magic "1INX", u16 version(1), u16 nameLen, name bytes, u32 count,
    // then for each entry: u16 keyLen, key bytes, u32 recno
    const std::string exprName = fieldTok; // store original token
    out.write("1INX", 4);
    wr_u16(out, 1);
    wr_u16(out, static_cast<uint16_t>(exprName.size()));
    out.write(exprName.data(), static_cast<std::streamsize>(exprName.size()));
    wr_u32(out, static_cast<uint32_t>(ents.size()));
    for (const auto& e : ents) {
        uint16_t klen = static_cast<uint16_t>(std::min<size_t>(e.key.size(), 0xFFFFu));
        wr_u16(out, klen);
        out.write(e.key.data(), klen);
        wr_u32(out, e.recno);
    }
    out.flush();

    std::cout << "Index written: " << outPath.filename().string()
              << "  (expr: " << fieldTok << ", ASC)\n";
}
