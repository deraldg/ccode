// ============================================================================
// File: src/cli/cmd_cmdhelpchk.cpp
// Robust validator for commands.dbf/.dbt produced by CMDHELP
// ============================================================================
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"

// Optional setpath-aware slot resolving (HELP slot).
#if __has_include("cli/path_resolver.hpp") && __has_include("cli/cmd_setpath.hpp")
  #include "cli/path_resolver.hpp"
  #include "cli/cmd_setpath.hpp"
  #define HAVE_PATHS 1
#else
  #define HAVE_PATHS 0
#endif
using xbase::DbArea;
namespace fs = std::filesystem;

namespace {


static fs::path help_root_dir() {
#if HAVE_PATHS
    try {
        namespace paths = dottalk::paths;
        return paths::get_slot(paths::Slot::HELP);
    } catch (...) {}
#endif
    std::error_code ec;
    fs::path cand = fs::current_path(ec) / "help";
    if (!ec && fs::exists(cand, ec) && fs::is_directory(cand, ec)) return cand;
    fs::path cwd = fs::current_path(ec);
    return ec ? fs::path{} : cwd;
}

static fs::path resolve_help_dir_arg(const std::string& argRaw) {
    std::string a = argRaw;
    while (!a.empty() && std::isspace((unsigned char)a.front())) a.erase(a.begin());
    while (!a.empty() && std::isspace((unsigned char)a.back())) a.pop_back();
    if (a.empty() || a == ".") return help_root_dir();
    fs::path p(a);
    if (p.is_absolute()) return p;
    return fs::weakly_canonical(help_root_dir() / p);
}

#pragma pack(push,1)
struct DbfHeader {
    uint8_t  version;
    uint8_t  y, m, d;
    uint32_t nrecs;
    uint16_t header_len;
    uint16_t rec_len;
    uint8_t  reserved[20];
};
struct DbfField {
    char     name[11];
    char     type;      // 'C','L','N','M'
    uint32_t data_addr;
    uint8_t  length;
    uint8_t  decimals;
    uint8_t  reserved[14];
};
#pragma pack(pop)

struct Col {
    std::string name;
    char        type{};
    uint8_t     len{};
    size_t      offset{};
};

static std::string trimr(std::string s) { while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back(); return s; }
static std::string triml(std::string s) { size_t i=0; while (i<s.size() && std::isspace((unsigned char)s[i])) ++i; return s.substr(i); }
static std::string upper(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return char(std::toupper(c)); }); return s; }

static bool read_exact(std::ifstream& in, void* buf, size_t n) {
    in.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(n));
    return static_cast<size_t>(in.gcount()) == n;
}

struct Dbf {
    DbfHeader            hdr{};
    std::vector<Col>     cols;
    std::ifstream        in;
    size_t               rec0{};
};

static Dbf open_dbf(const fs::path& p) {
    Dbf dbf;
    dbf.in.open(p, std::ios::binary);
    if (!dbf.in) throw std::runtime_error("cannot open " + p.string());
    if (!read_exact(dbf.in, &dbf.hdr, sizeof(dbf.hdr))) throw std::runtime_error("short read header");

    // read fields until 0x0D
    while (true) {
        char c;
        if (!dbf.in.read(&c, 1)) throw std::runtime_error("short read fields");
        if (static_cast<unsigned char>(c) == 0x0D) break;
        DbfField f{}; f.name[0] = c;
        if (!read_exact(dbf.in, reinterpret_cast<char*>(&f)+1, sizeof(DbfField)-1)) throw std::runtime_error("short read field");

        Col col;
        col.name.assign(f.name, f.name + 11);
        col.name = trimr(col.name);
        col.type = f.type;
        col.len  = f.length;
        dbf.cols.push_back(col);
    }
    // compute offsets
    size_t off = 1;
    for (auto& c : dbf.cols) { c.offset = off; off += c.len; }
    dbf.rec0 = static_cast<size_t>(dbf.hdr.header_len);
    return dbf;
}

static std::string memo_preview(const fs::path& dbt_path, uint32_t start_block, size_t& out_len, size_t max_chars = 240) {
    out_len = 0;
    if (start_block == 0) return {};
    std::ifstream dbt(dbt_path, std::ios::binary);
    if (!dbt) return {};
    const uint64_t base = uint64_t(start_block) * 512ull;
    dbt.seekg(static_cast<std::streamoff>(base), std::ios::beg);
    uint8_t lenbuf[4]{};
    if (!dbt.read(reinterpret_cast<char*>(lenbuf), 4)) return {};
    uint32_t len = (uint32_t(lenbuf[0]) | (uint32_t(lenbuf[1]) << 8) | (uint32_t(lenbuf[2]) << 16) | (uint32_t(lenbuf[3]) << 24));
    out_len = len;

    std::string s; s.resize(std::min<size_t>(len, max_chars));
    if (!dbt.read(&s[0], static_cast<std::streamsize>(s.size()))) s.clear();
    for (auto& ch : s) if ((unsigned char)ch < 0x20 && ch!='\n' && ch!='\r' && ch!='\t') ch = ' ';
    return s;
}

static std::string read_char(const std::vector<char>& rec, const Col& c) {
    std::string s(rec.data() + c.offset, rec.data() + c.offset + c.len);
    return trimr(s);
}
static uint32_t read_mptr(const std::vector<char>& rec, const Col& c) {
    std::string s(rec.data() + c.offset, rec.data() + c.offset + c.len);
    s = triml(trimr(s));
    if (s.empty()) return 0;
    return static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
}

static std::pair<fs::path,fs::path> resolve_paths(const fs::path& outdir) {
    fs::path p1 = outdir / "commands.dbf";
    if (fs::exists(p1)) return {p1, outdir / "commands.dbt"};
    fs::path p2 = outdir / "data" / "commands.dbf";
    if (fs::exists(p2)) return {p2, outdir / "data" / "commands.dbt"};
    throw std::runtime_error("commands.dbf not found in '" + (outdir.string()) + "' or '" + (outdir / "data").string() + "'");
}

} // anon

void cmd_CMDHELPCHK(DbArea& /*area*/, std::istringstream& in) {
    std::string outdir; std::getline(in, outdir);
    fs::path outdir_p = resolve_help_dir_arg(outdir);
    outdir = outdir_p.string();
    std::string limit_s; std::getline(in, limit_s);
    int limit = 5; if (!limit_s.empty()) { try { limit = std::max(1, std::stoi(limit_s)); } catch(...){} }

    try {
        auto [dbf_path, dbt_path] = resolve_paths(outdir);
        auto dbf = open_dbf(dbf_path);

        // Discover columns
        int id_ix=-1, cmd_ix=-1, usage_ix=-1, verbose_ix=-1;
        std::vector<int> memo_ix;
        for (int i=0;i<(int)dbf.cols.size();++i) {
            const auto n = upper(dbf.cols[i].name);
            if (n=="ID") id_ix = i;
            else if (n=="COMMAND") cmd_ix = i;
            else if (n=="USAGE") usage_ix = i;
            else if (n=="VERBOSE") verbose_ix = i;
            if (dbf.cols[i].type == 'M') memo_ix.push_back(i);
        }
        // Fallback: if USAGE/VERBOSE names not present, pick first two memos
        if (usage_ix < 0 && !memo_ix.empty()) usage_ix = memo_ix[0];
        if (verbose_ix < 0 && memo_ix.size() >= 2) verbose_ix = memo_ix[1];

        // Print discovered header
        std::cout << "Opened " << dbf_path.string() << " (" << dbf.hdr.nrecs << " rows)\n";
        std::cout << "Columns:";
        for (auto& c : dbf.cols) std::cout << " [" << c.name << ":" << c.type << "," << int(c.len) << "]";
        std::cout << "\n";

        if (id_ix<0 || cmd_ix<0 || usage_ix<0) {
            std::cout << "Missing expected columns; need at least ID, COMMAND, and one memo (USAGE)\n";
            return;
        }

        std::cout << std::left
                  << std::setw(6)  << "ID"
                  << std::setw(20) << "COMMAND"
                  << std::setw(14) << (std::string(dbf.cols[usage_ix].name) + "(len)")
                  << "Memo preview\n";
        std::cout << "-----------------------------------------------------------------------------\n";

        dbf.in.seekg(static_cast<std::streamoff>(dbf.rec0), std::ios::beg);
        std::vector<char> rec(dbf.hdr.rec_len);
        int shown = 0;
        for (uint32_t r=0; r<dbf.hdr.nrecs && shown<limit; ++r) {
            if (!read_exact(dbf.in, rec.data(), rec.size())) break;
            if (rec[0] == '*') continue;

            const auto& C_id = dbf.cols[id_ix];
            const auto& C_nm = dbf.cols[cmd_ix];
            const auto& C_u  = dbf.cols[usage_ix];

            auto id_str  = read_char(rec, C_id);
            auto cmd_str = read_char(rec, C_nm);
            auto u_blk   = read_mptr(rec, C_u);

            size_t u_len=0;
            auto u_prev = memo_preview(dbt_path, u_blk, u_len);

            std::cout << std::left
                      << std::setw(6)  << id_str
                      << std::setw(20) << cmd_str
                      << std::setw(14) << (std::to_string(u_len))
                      << u_prev << "\n";

            // Optional second memo column preview (VERBOSE or next memo)
            if (verbose_ix >= 0) {
                auto v_blk   = read_mptr(rec, dbf.cols[verbose_ix]);
                size_t v_len = 0;
                auto v_prev  = memo_preview(dbt_path, v_blk, v_len);
                if (!v_prev.empty()) {
                    std::cout << std::setw(26) << " "
                              << std::setw(14) << (std::to_string(v_len))
                              << v_prev << "\n";
                }
            }
            ++shown;
        }
        if (shown==0) std::cout << "(no rows shown; try increasing limit)\n";
        std::cout << "DBT: " << dbt_path.string() << "\n";
    } catch (const std::exception& ex) {
        std::cout << "CMDHELPCHK error: " << ex.what() << "\n";
    }
}



