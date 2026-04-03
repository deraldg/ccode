// src/cli/cmd_reindex.cpp
// REINDEX [INX|CNX|CDX|SIX|SCX|ALL|CUSTOM] [family-specific args...]
//
// Canonical rebuild dispatcher.
//
// Family policy:
// - REINDEX INX   -> rebuild INX
// - REINDEX CNX   -> rebuild CNX (via legacy REBUILD path)
// - REINDEX CDX   -> rebuild CDX/LMDB (via BUILDLMDB dev power tool)
// - REINDEX SIX   -> student single-tag stub index
// - REINDEX SCX   -> student compound stub index
// - REINDEX ALL   -> v32 table: INX + CNX, v64 table: CDX
// - REINDEX CUSTOM-> SIX + SCX
//
// Default policy:
// - REINDEX with no args chooses by open table flavor:
//     v64-like table -> CDX
//     v32-like table -> INX
// - If no table is open, fallback default is CDX.
//
// Notes:
// - CUSTOM is the command token; the internal enum name avoids "CUSTOM"
//   directly to dodge macro/name collisions on some builds.
// - Console output is ASCII-only (CP437-safe).
// - REINDEX ALL excludes student families by design.
//   Student families are opt-in through REINDEX CUSTOM.

#include "xbase.hpp"
#include "cli/path_resolver.hpp"
#include "cli/cmd_setpath.hpp"
#include "cli/table_state.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using xbase::DbArea;

extern "C" xbase::XBaseEngine* shell_engine(void);

// forward declare (defined elsewhere)
void cmd_COMMIT(xbase::DbArea& A, std::istringstream& in);
void cmd_REBUILD(xbase::DbArea& A, std::istringstream& in);
void cmd_BUILDLMDB(xbase::DbArea& A, std::istringstream& in);

// ----------------------------- helpers ---------------------------------------

namespace {

enum class ReindexFamily {
    Default,
    INX,
    CNX,
    CDX,
    SIX,
    SCX,
    ALL,
    CustomGroup
};

static std::string trim_copy(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string upper_copy(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static bool prompt_yes_no(const std::string& prompt, bool default_no = true) {
    std::cout << prompt;
    std::cout << (default_no ? " (y/N) " : " (Y/n) ");
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return !default_no;
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(line[0])));
    return c == 'Y';
}

static int resolve_current_index(xbase::DbArea& A) {
    if (auto* eng = shell_engine()) {
        for (int i = 0; i < xbase::MAX_AREA; ++i) {
            if (&eng->area(i) == &A) return i;
        }
    }
    return -1;
}

static bool ensure_clean_or_commit(xbase::DbArea& A, int area0, const char* verb) {
    if (area0 < 0) return true;

    if (!dottalk::table::is_enabled(area0)) return true;
    if (!dottalk::table::is_dirty(area0)) return true;

    std::ostringstream oss;
    oss << verb << ": TABLE has uncommitted changes. Commit now and continue?";
    if (!prompt_yes_no(oss.str(), true)) {
        std::cout << verb << ": canceled (dirty table).\n";
        return false;
    }

    std::istringstream empty;
    cmd_COMMIT(A, empty);

    if (dottalk::table::is_dirty(area0)) {
        std::cout << verb << ": still dirty after COMMIT; canceling.\n";
        return false;
    }

    return true;
}

// Canonicalize name: strip spaces/tabs/NUL, keep [A-Za-z0-9_], uppercase.
static std::string canon(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\0' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\0' || s[e - 1] == '\t')) --e;

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

// Heuristic default-family chooser using APIs that actually exist in xbase.hpp.
// versionByte() and tableFlags() are public. A classic 32-bit DBF is commonly 0x03.
// The 64-bit / extended formats in this codebase carry extended flags and/or a
// non-classic version byte. If in doubt, default to INX only for plain classic DBF.
static bool looks_like_v64_table(const DbArea& A)
{
    if (!A.isOpen()) return false;

    if (A.tableFlags() != 0) return true;

    const uint8_t ver = A.versionByte();

    // Treat plain classic dBASE III (0x03) as v32.
    // Everything else is safer to regard as extended/v64-like for default dispatch.
    return ver != 0x03;
}

static ReindexFamily default_family_for_area(const DbArea& A)
{
    if (!A.isOpen()) {
        return ReindexFamily::CDX; // no-table fallback
    }
    return looks_like_v64_table(A) ? ReindexFamily::CDX : ReindexFamily::INX;
}

static void print_reindex_help()
{
    std::cout
        << "REINDEX [INX|CNX|CDX|SIX|SCX|ALL|CUSTOM] [family-specific args]\n"
        << "\n"
        << "Canonical rebuild command.\n"
        << "\n"
        << "  REINDEX\n"
        << "      Default family by open table flavor:\n"
        << "      v64-like table -> CDX (via BUILDLMDB)\n"
        << "      v32-like table -> INX\n"
        << "\n"
        << "  REINDEX INX [<tagfile>]\n"
        << "      Rebuild a legacy single-tag INX file.\n"
        << "\n"
        << "  REINDEX CNX [<name-or-path.cnx>]\n"
        << "      Rebuild a legacy compound CNX file.\n"
        << "\n"
        << "  REINDEX CDX [YES|AUTO|NOPROMPT] [CLEAN|FORCE] [QUIET]\n"
        << "      Rebuild CDX/LMDB backing store via BUILDLMDB.\n"
        << "\n"
        << "  REINDEX SIX [<tagfile>]\n"
        << "      Student single-tag family.\n"
        << "\n"
        << "  REINDEX SCX [<tagfile>]\n"
        << "      Student compound family.\n"
        << "\n"
        << "  REINDEX ALL\n"
        << "      v32-like table -> INX + CNX\n"
        << "      v64-like table -> CDX\n"
        << "      (excludes SIX/SCX by design)\n"
        << "\n"
        << "  REINDEX CUSTOM\n"
        << "      SIX + SCX\n"
        << "\n"
        << "Backward compatibility:\n"
        << "  REINDEX <tagfile>\n"
        << "      Treated as REINDEX INX <tagfile>.\n";
}

// little-endian I/O
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
static void wr_i32(std::ofstream& out, int32_t v) {
    wr_u32(out, static_cast<uint32_t>(v));
}

static bool rd_u16(std::ifstream& in, uint16_t& v) {
    unsigned char b[2];
    if (!in.read(reinterpret_cast<char*>(b), 2)) return false;
    v = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
    return true;
}

static bool read_inx_expr_token(const fs::path& tagFile, std::string& exprTokOut, uint16_t& verOut) {
    std::ifstream in(tagFile, std::ios::binary);
    if (!in) return false;

    char magic[4]{};
    if (!in.read(magic, 4)) return false;

    if (std::memcmp(magic, "1INX", 4) == 0) {
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

    if (std::memcmp(magic, "2INX", 4) == 0) {
        uint16_t ver = 0, keylen = 0, nameLen = 0;
        if (!rd_u16(in, ver)) return false;
        if (!rd_u16(in, keylen)) return false;
        (void)keylen;
        if (!rd_u16(in, nameLen)) return false;

        std::string name;
        name.resize(nameLen);
        if (!in.read(name.data(), static_cast<std::streamsize>(nameLen))) return false;

        exprTokOut = name;
        verOut = ver;
        return true;
    }

    return false;
}

// Build (rewrite) the index file for the given expression token.
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

    const auto& fdef = Fs[static_cast<size_t>(fldIdx - 1)];
    const uint16_t keylen = static_cast<uint16_t>(fdef.length);
    const char ftype = fdef.type;

    struct Entry { std::string key; uint32_t recno; };
    std::vector<Entry> ents;
    ents.reserve(static_cast<size_t>(std::max(0, A.recCount())));

    const int32_t total = A.recCount();
    for (int32_t rn = 1; rn <= total; ++rn) {
        if (!A.gotoRec(rn)) continue;
        if (!A.readCurrent()) continue;
        if (A.isDeleted()) continue;

        std::string k = A.get(fldIdx);
        if (ftype == 'C' || ftype == 'c') {
            k = upper_copy(k);
        }
        if (k.size() > keylen) k.resize(keylen);
        if (k.size() < keylen) k.append(static_cast<size_t>(keylen - k.size()), ' ');

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

    std::vector<int32_t> pos_by_recno(static_cast<size_t>(total) + 1u, -1);
    for (uint32_t i = 0; i < static_cast<uint32_t>(ents.size()); ++i) {
        const uint32_t rn = ents[i].recno;
        if (rn <= static_cast<uint32_t>(total)) pos_by_recno[rn] = static_cast<int32_t>(i);
    }

    outFile.write("2INX", 4);
    wr_u16(outFile, 2);
    wr_u16(outFile, keylen);
    wr_u16(outFile, static_cast<uint16_t>(exprTok.size()));
    outFile.write(exprTok.data(), static_cast<std::streamsize>(exprTok.size()));
    wr_u32(outFile, static_cast<uint32_t>(ents.size()));
    wr_u32(outFile, static_cast<uint32_t>(total));

    for (const auto& e : ents) {
        outFile.write(e.key.data(), static_cast<std::streamsize>(e.key.size()));
        wr_u32(outFile, e.recno);
    }

    for (size_t i = 0; i < pos_by_recno.size(); ++i) {
        wr_i32(outFile, pos_by_recno[i]);
    }

    outFile.flush();

    std::cout << "REINDEX: wrote " << out.filename().string()
              << "  (2INX v2, expr: " << exprTok << ", ASC)\n";
    return true;
}

static fs::path probe_default_inx_for_current(DbArea& A) {
    fs::path nm(A.name());
    std::string base = nm.stem().string();
    if (base.empty()) return {};

    fs::path p_indexes = dottalk::paths::resolve_index(base + ".inx");
    if (fs::exists(p_indexes)) return p_indexes;

    return p_indexes;
}

static bool run_reindex_inx(DbArea& A, const std::string& argRest)
{
    const int area0 = resolve_current_index(A);
    if (!ensure_clean_or_commit(A, area0, "REINDEX")) return false;

    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        return false;
    }

    fs::path tagPath;
    if (!argRest.empty()) {
        tagPath = dottalk::paths::resolve_index(argRest);
        tagPath = dottalk::paths::ensure_ext(std::move(tagPath), ".inx");
    } else {
        tagPath = probe_default_inx_for_current(A);
        if (tagPath.empty()) {
            std::cout << "REINDEX: cannot infer tag path (unknown table name).\n"
                         "Specify a tag file: REINDEX INX <tagfile.inx>\n";
            return false;
        }
    }

    if (!tagPath.has_extension()) tagPath.replace_extension(".inx");

    if (!fs::exists(tagPath)) {
        std::cout << "REINDEX: tag file not found: " << tagPath.string() << "\n";
        std::cout << "Hint: create it with: INDEX ON <field|#n> TAG "
                  << tagPath.filename().string() << "\n";
        return false;
    }

    std::string exprTok;
    uint16_t ver = 0;
    if (!read_inx_expr_token(tagPath, exprTok, ver)) {
        std::cout << "REINDEX: cannot read tag expression from: " << tagPath.string() << "\n";
        return false;
    }

    std::cout << "REINDEX INX\n";
    std::cout << "  Index file   : " << tagPath.string() << "\n";
    std::cout << "  Tag expr     : " << exprTok << "\n";

    if (!build_inx(A, exprTok, tagPath)) {
        std::cout << "REINDEX: failed.\n";
        return false;
    }

    if (area0 >= 0 && dottalk::table::is_enabled(area0)) {
        dottalk::table::set_stale(area0, false);
        dottalk::table::clear_stale_fields(area0);
        std::cout << "REINDEX: TABLE STALE cleared (fresh).\n";
    }

    std::cout << "Note: INX file was regenerated from its stored tag expression.\n";
    return true;
}

static bool run_reindex_cnx(DbArea& A, const std::string& argRest)
{
    std::istringstream in(argRest);
    cmd_REBUILD(A, in);
    return true;
}

static bool run_reindex_cdx(DbArea& A, const std::string& argRest)
{
    std::istringstream in(argRest);
    cmd_BUILDLMDB(A, in);
    return true;
}

static void run_reindex_six(DbArea& A, const std::string& argRest)
{
    std::cout << "REINDEX SIX (student single-tag)\n";
    std::cout << "  Table : " << (A.isOpen() ? A.name() : "(no table open)") << "\n";
    if (!argRest.empty()) {
        std::cout << "  Arg   : " << argRest << "\n";
    }
    std::cout << "  Status: stub (no backend)\n";
}

static void run_reindex_scx(DbArea& A, const std::string& argRest)
{
    std::cout << "REINDEX SCX (student compound)\n";
    std::cout << "  Table : " << (A.isOpen() ? A.name() : "(no table open)") << "\n";
    if (!argRest.empty()) {
        std::cout << "  Arg   : " << argRest << "\n";
    }
    std::cout << "  Status: stub (no backend)\n";
}

static bool dispatch_all(DbArea& A, const std::string& rest)
{
    if (default_family_for_area(A) == ReindexFamily::CDX) {
        std::cout << "REINDEX ALL -> CDX (v64-like table)\n";
        return run_reindex_cdx(A, rest);
    }

    std::cout << "REINDEX ALL -> INX + CNX (v32-like table)\n";

    bool ok_inx = run_reindex_inx(A, std::string());
    bool ok_cnx = run_reindex_cnx(A, rest);

    return ok_inx && ok_cnx;
}

static void dispatch_custom(DbArea& A, const std::string& rest)
{
    std::cout << "REINDEX CUSTOM -> SIX + SCX\n";
    run_reindex_six(A, rest);
    run_reindex_scx(A, rest);
}

} // namespace

// ------------------------------- command -------------------------------------

void cmd_REINDEX(DbArea& A, std::istringstream& args) {
    std::string argRest;
    std::getline(args, argRest);
    argRest = trim_copy(argRest);

    if (argRest.empty()) {
        const ReindexFamily def = default_family_for_area(A);
        if (def == ReindexFamily::CDX) {
            std::cout << "REINDEX default -> CDX (v64-like table, via BUILDLMDB)\n";
            run_reindex_cdx(A, std::string());
        } else {
            std::cout << "REINDEX default -> INX (v32-like table)\n";
            run_reindex_inx(A, std::string());
        }
        return;
    }

    std::istringstream peek(argRest);
    std::string firstTok;
    peek >> firstTok;
    const std::string firstUp = upper_copy(firstTok);

    std::string restAfterFirst;
    std::getline(peek, restAfterFirst);
    restAfterFirst = trim_copy(restAfterFirst);

    if (firstUp == "HELP" || firstUp == "?" || firstUp == "/?" ||
        firstUp == "-H" || firstUp == "--HELP") {
        print_reindex_help();
        return;
    }

    if (firstUp == "INX") {
        run_reindex_inx(A, restAfterFirst);
        return;
    }

    if (firstUp == "CNX") {
        std::cout << "REINDEX CNX\n";
        run_reindex_cnx(A, restAfterFirst);
        return;
    }

    if (firstUp == "CDX") {
        std::cout << "REINDEX CDX -> BUILDLMDB\n";
        run_reindex_cdx(A, restAfterFirst);
        return;
    }

    if (firstUp == "SIX") {
        run_reindex_six(A, restAfterFirst);
        return;
    }

    if (firstUp == "SCX") {
        run_reindex_scx(A, restAfterFirst);
        return;
    }

    if (firstUp == "ALL") {
        dispatch_all(A, restAfterFirst);
        return;
    }

    if (firstUp == "CUSTOM") {
        dispatch_custom(A, restAfterFirst);
        return;
    }

    // Backward-compatible legacy form:
    // REINDEX <tagfile> -> treat as INX operand.
    run_reindex_inx(A, argRest);
}