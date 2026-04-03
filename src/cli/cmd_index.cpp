// src/cli/cmd_index.cpp
//
// INDEX ON <field|#n> TAG <name> [ASC|DESC] [1INX|2INX]
//   - Default direction: ASC
//   - Default output format: 2INX (matches REINDEX).
//   - Optional direction token selects ASC or DESC.
//   - Optional format token selects 1INX or 2INX.
//
// Notes:
//   - Deleted records are excluded (matches REINDEX).
//   - 2INX uses fixed-length keys (field length), uppercased for character fields,
//     and includes a pos-by-recno table for fast navigation.
//
// Examples:
//   INDEX ON LNAME TAG students
//   INDEX ON LNAME TAG students DESC
//   INDEX ON #2 TAG students ASC 1INX
//   INDEX ON LNAME TAG students DESC 2INX

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/path_resolver.hpp"
#include "cli/cmd_setpath.hpp"
#include "cli/index_utils.hpp"          // shared index utilities & abstractions

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

namespace {

// ────────────────────────────────────────────────
// Binary write helpers – placed FIRST so they are visible to write_1inx / write_2inx
// ────────────────────────────────────────────────
void wr_u16(std::ofstream& out, uint16_t v) {
    unsigned char b[2] = {
        static_cast<unsigned char>(v & 0xFF),
        static_cast<unsigned char>((v >> 8) & 0xFF)
    };
    out.write(reinterpret_cast<const char*>(b), 2);
}

void wr_u32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = {
        static_cast<unsigned char>( v        & 0xFF),
        static_cast<unsigned char>((v >> 8)  & 0xFF),
        static_cast<unsigned char>((v >> 16) & 0xFF),
        static_cast<unsigned char>((v >> 24) & 0xFF)
    };
    out.write(reinterpret_cast<const char*>(b), 4);
}

void wr_i32(std::ofstream& out, int32_t v) {
    wr_u32(out, static_cast<uint32_t>(v));
}

// ────────────────────────────────────────────────
// Parsing & validation helpers
// ────────────────────────────────────────────────

bool parse_fmt_token(const std::string& tok, dottalk::InxFmt& fmt_out) {
    const std::string t = dottalk::canon(tok);
    if (t == "1INX" || t == "1" || t == "V1") { fmt_out = dottalk::InxFmt::V1_1INX; return true; }
    if (t == "2INX" || t == "2" || t == "V2") { fmt_out = dottalk::InxFmt::V2_2INX; return true; }
    return false;
}

// ASC/DESC support added here.
// This keeps direction parsing local to INDEX so the command can be self-contained.
bool parse_dir_token(const std::string& tok, bool& descending_out) {
    const std::string t = dottalk::canon(tok);
    if (t == "ASC" || t == "ASCEND" || t == "ASCENDING") {
        descending_out = false;
        return true;
    }
    if (t == "DESC" || t == "DESCEND" || t == "DESCENDING") {
        descending_out = true;
        return true;
    }
    return false;
}

// Parse:
//   INDEX ON <field|#n> TAG <filename> [ASC|DESC] [1INX|2INX]
//
// Defaults:
//   direction = ASC
//   format    = 2INX
//
// Notes:
//   - Preferred syntax is [ASC|DESC] [1INX|2INX].
//   - For robustness, this parser accepts the two optional tokens in either order,
//     but only one direction token and one format token may appear.
bool parse_args(std::istringstream& in,
                std::string& field,
                std::string& tag,
                bool& descending_out,
                dottalk::InxFmt& fmt_out) {
    descending_out = false;                    // default ASC
    fmt_out = dottalk::InxFmt::V2_2INX;        // default 2INX

    std::string onTok;
    if (!(in >> onTok)) return false;
    if (dottalk::canon(onTok) != "ON") return false;

    if (!(in >> field) || field.empty()) return false;

    std::string tagTok;
    if (!(in >> tagTok)) return false;
    if (dottalk::canon(tagTok) != "TAG") return false;

    if (!(in >> tag) || tag.empty()) return false;

    bool saw_dir = false;
    bool saw_fmt = false;

    std::string opt;
    while (in >> opt) {
        bool parsed = false;

        if (!saw_dir) {
            bool desc_tmp = false;
            if (parse_dir_token(opt, desc_tmp)) {
                descending_out = desc_tmp;
                saw_dir = true;
                parsed = true;
            }
        }

        if (!parsed && !saw_fmt) {
            dottalk::InxFmt fmt_tmp = fmt_out;
            if (parse_fmt_token(opt, fmt_tmp)) {
                fmt_out = fmt_tmp;
                saw_fmt = true;
                parsed = true;
            }
        }

        if (!parsed) {
            return false;
        }
    }

    return true;
}

bool tag_token_allowed(const std::string& tagTok) {
    fs::path p(tagTok);
    if (p.is_absolute()) return true;
    if (!p.has_parent_path()) return true;  // bare name is fine

    auto it = p.begin();
    if (it == p.end()) return false;
    const std::string first = dottalk::lower_copy(it->string());
    return (first == "dbf" || first == "indexes");
}

// ────────────────────────────────────────────────
// Index file writers
// ────────────────────────────────────────────────

bool write_1inx(const fs::path& outPath, const std::string& exprTok, const std::vector<dottalk::Entry>& ents) {
    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out.write("1INX", 4);
    wr_u16(out, 1);
    wr_u16(out, static_cast<uint16_t>(exprTok.size()));
    out.write(exprTok.data(), static_cast<std::streamsize>(exprTok.size()));
    wr_u32(out, static_cast<uint32_t>(ents.size()));
    for (const auto& e : ents) {
        const uint16_t klen = static_cast<uint16_t>(std::min<std::size_t>(e.key.size(), 0xFFFFu));
        wr_u16(out, klen);
        out.write(e.key.data(), klen);
        wr_u32(out, e.recno);
    }
    out.flush();
    return true;
}

bool write_2inx(const fs::path& outPath,
                const std::string& exprTok,
                uint16_t keylen,
                int32_t recCountSnapshot,
                const std::vector<dottalk::Entry>& ents) {
    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    // Build pos-by-recno table: int32[recCount+1]
    std::vector<int32_t> pos_by_recno(static_cast<std::size_t>(recCountSnapshot) + 1u, -1);
    for (uint32_t i = 0; i < static_cast<uint32_t>(ents.size()); ++i) {
        const uint32_t rn = ents[i].recno;
        if (rn <= static_cast<uint32_t>(recCountSnapshot)) {
            pos_by_recno[rn] = static_cast<int32_t>(i);
        }
    }

    out.write("2INX", 4);
    wr_u16(out, 2);
    wr_u16(out, keylen);
    wr_u16(out, static_cast<uint16_t>(exprTok.size()));
    out.write(exprTok.data(), static_cast<std::streamsize>(exprTok.size()));
    wr_u32(out, static_cast<uint32_t>(ents.size()));
    wr_u32(out, static_cast<uint32_t>(recCountSnapshot));

    for (const auto& e : ents) {
        out.write(e.key.data(), static_cast<std::streamsize>(e.key.size()));
        wr_u32(out, e.recno);
    }
    for (std::size_t i = 0; i < pos_by_recno.size(); ++i) {
        wr_i32(out, pos_by_recno[i]);
    }

    out.flush();
    return true;
}

} // anonymous namespace

void cmd_INDEX(DbArea& A, std::istringstream& in)
{
    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    std::string fieldTok, tag;
    bool descending = false;                          // ASC default
    dottalk::InxFmt fmt = dottalk::InxFmt::V2_2INX;  // 2INX default

    if (!parse_args(in, fieldTok, tag, descending, fmt)) {
        std::cout << "Usage: INDEX ON <field|#n> TAG <name> [ASC|DESC] [1INX|2INX]\n";
        std::cout << "Defaults: ASC, 2INX\n";
        std::cout << "Examples:\n";
        std::cout << "  INDEX ON LNAME TAG students\n";
        std::cout << "  INDEX ON LNAME TAG students DESC\n";
        std::cout << "  INDEX ON #2 TAG students ASC 1INX\n";
        return;
    }

    // If you later want INDEX with no explicit ASC/DESC token to inherit the
    // current global/session ASCEND/DESCEND mode, this is the place to do it.
    //
    // Example future hook:
    //   if (!explicit_dir_supplied) descending = current_session_descending();
    //
    // Right now, the command default is self-contained:
    //   no direction token => ASC

    if (!tag_token_allowed(tag)) {
        std::cout << "INDEX: invalid TAG path '" << tag << "'.\n";
        std::cout << "Use a bare name (TAG students), an absolute path, or a slot path:\n";
        std::cout << "  TAG indexes/students   or   TAG dbf/students\n";
        return;
    }

    const int fldIdx = dottalk::resolve_field_index(A, fieldTok);
    if (fldIdx < 1) {
        std::cout << "INDEX: unknown field '" << fieldTok << "'.\n";
        std::cout << "Available:\n";
        const auto& Fs = A.fields();
        for (std::size_t i = 0; i < Fs.size(); ++i) {
            std::cout << "  " << Fs[i].name << "\n";
        }
        std::cout << "Tip: INDEX ON #3 TAG students\n";
        return;
    }

    uint16_t keylen = 0;
    char ftype = 0;
    std::vector<dottalk::Entry> ents = dottalk::create_sorted_entries(
        A, fldIdx, fmt, keylen, ftype
        // sorter defaults to std_sort_algo
    );

    // ASC/DESC support added here.
    // create_sorted_entries() returns ascending order, so DESC is applied by reversal.
    // This keeps the shared entry-builder simple and makes DESC a command-level choice.
    if (descending) {
        std::reverse(ents.begin(), ents.end());
    }

    fs::path outPath = dottalk::paths::resolve_index(tag);
    outPath = dottalk::paths::ensure_ext(outPath, ".inx");

    if (outPath.has_extension() && dottalk::lower_copy(outPath.extension().string()) != ".inx") {
        std::cout << "INDEX: TAG must name an .inx file.\n";
        std::cout << "Got: " << outPath.string() << "\n";
        return;
    }

    const std::string exprTok = fieldTok; // store original token (e.g. "LNAME" or "#2")
    const int32_t recSnap = A.recCount();

    bool ok = false;
    if (fmt == dottalk::InxFmt::V1_1INX) {
        ok = write_1inx(outPath, exprTok, ents);
    } else {
        ok = write_2inx(outPath, exprTok, keylen, recSnap, ents);
    }

    if (!ok) {
        std::cout << "INDEX: cannot write file: " << outPath.string() << "\n";
        return;
    }

    std::cout << "Index written: " << outPath.filename().string()
              << "  (" << (fmt == dottalk::InxFmt::V1_1INX ? "1INX" : "2INX")
              << ", expr: " << fieldTok
              << ", " << (descending ? "DESC" : "ASC") << ")\n";
}