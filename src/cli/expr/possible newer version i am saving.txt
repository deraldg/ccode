// ============================================================================
// File: src/cli/cmd_cmdhelp.cpp
// ============================================================================
// Adds mode switch:
//   - BUILD  : build DBFs (same behavior as before), then print report
//   - (none) : show existing DBFs (commands.dbf/cmd_args.dbf) in same report
//
#include "cmd_cmdhelp.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "xbase.hpp"
#include "edref.hpp"

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

namespace { // ==== utils & miners ==================================================

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

// Resolve an output/read directory argument for help DBFs.
// - empty / "." => HELP slot directory
// - absolute => unchanged
// - relative => interpreted relative to HELP slot directory
static fs::path resolve_help_dir_arg(const std::string& argRaw) {
    std::string a = argRaw;
    while (!a.empty() && std::isspace((unsigned char)a.front())) a.erase(a.begin());
    while (!a.empty() && std::isspace((unsigned char)a.back())) a.pop_back();
    if (a.empty() || a == ".") return help_root_dir();
    fs::path p(a);
    if (p.is_absolute()) return p;
    return fs::weakly_canonical(help_root_dir() / p);
}

std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static std::string to_string_or_empty(const char* p) {
    return p ? std::string(p) : std::string();
}

static std::string make_cmdkey(const std::string& catalog, const std::string& command) {
    return up(catalog) + "|" + up(command);
}

// FoxRef syntax -> likely switches
std::vector<std::string> switches_from_syntax(const std::string& syntax) {
    static const std::unordered_set<std::string> IGN = {
        "USE","FIELDS","INDEX","SETINDEX","LIST","FIND","SEEK","GOTO","TOP","BOTTOM","APPEND","APPEND BLANK",
        "REPLACE","DELETE","RECALL","PACK","EXPORT","IMPORT","STRUCT","STATUS","COUNT","SET","ORDER","TO","INTO",
        "<TABLE>","<FLD>","<FIELD>","<VALUE>","<CSV>","<N>","<TAG>","<EXPR>","#N","<SPEC>","<NAME>","<RECNO>",
        "<VAL>","<FIELD|#N>","<TAG|PATH>","<OP>","<AREA>","IN","BY","WHERE","GROUP","UNIQUE","ASC","DESC","ALIAS"
    };
    static const std::unordered_set<std::string> LIKELY = {
        "ALL","VERBOSE","TAG","INDEX","NOINDEX","FOR","WHILE","NEXT","RECORD","REST",
        "ASC","DESC","UNIQUE","FIELDS","ALIAS","ON","OFF"
    };

    std::vector<std::string> out;
    std::string t;
    for (char c : syntax) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            t.push_back(static_cast<char>(std::toupper(c)));
        } else {
            if (!t.empty()) {
                if (!IGN.count(t) && LIKELY.count(t)) out.push_back(t);
                t.clear();
            }
        }
    }
    if (!t.empty()) {
        if (!IGN.count(t) && LIKELY.count(t)) out.push_back(t);
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::string command_hint_from_path(const fs::path& p) {
    std::regex rx(R"(cmd_([a-zA-Z0-9_]+))");
    std::smatch m;
    const std::string base = p.stem().string();
    if (std::regex_search(base, m, rx) && m.size() > 1) {
        std::string n = m[1].str();
        std::replace(n.begin(), n.end(), '_', ' ');
        return up(n);
    }
    return {};
}

// Broader mining: tokens, NO* toggles, SET <OPT> ON|OFF, string flags
std::unordered_map<std::string, std::set<std::string>>
scan_sources_for_switches(const std::vector<std::string>& roots)
{
    const std::regex rx_case_token(R"(\bcase\s+(?:Token::|TOK_|TOKEN_)([A-Z][A-Z0-9_]+)\b)");
    const std::regex rx_token(R"(\b(?:Token::|TOK_|TOKEN_)([A-Z][A-Z0-9_]+)\b)");
    const std::regex rx_str(R"(["']([A-Z][A-Z0-9_]{1,24})["'])");
    const std::regex rx_has(R"(\b(?:hasSwitch|hasFlag|is_kw|has_kw|seenSwitch)\s*\(\s*(?:["']([A-Z][A-Z0-9_]{1,24})["']|(?:Token::|TOK_|TOKEN_)([A-Z][A-Z0-9_]+))\s*\))");
    const std::regex rx_reg(R"(register_?command\s*\(\s*["']([A-Z][A-Z0-9_ ]+)["'])");
    const std::regex rx_set(R"(\bSET\s+([A-Z][A-Z0-9_]+)\s+(ON|OFF)\b)");
    const std::regex rx_no(R"(\bNO([A-Z][A-Z0-9_]+)\b)");

    static const std::unordered_set<std::string> SWITCHLIKE = {
        "ALL","VERBOSE","TAG","INDEX","NOINDEX","FOR","WHILE","NEXT","RECORD","REST",
        "ASC","DESC","UNIQUE","FIELDS","ALIAS","INTO","TO","OFF","ON"
    };
    static const std::unordered_set<std::string> EXCLUDE_CMDS = {
        "USE","SCAN","ENDSCAN","SELECT","DELETE","REPLACE","INSERT","UPDATE","BROWSE","COUNT","SET","ORDER",
        "TOP","BOTTOM","GOTO","SEEK","FIND","APPEND","STRUCT","STATUS","PACK","ZAP","INDEX","SETINDEX"
    };

    std::unordered_map<std::string, std::set<std::string>> per_cmd;

    auto consider = [&](const std::string& cmd, const std::string& tok) {
        if (cmd.empty()) return;
        std::string t = up(tok);
        if (EXCLUDE_CMDS.count(t)) return;
        if (t.rfind("NO", 0) == 0 || SWITCHLIKE.count(t)) per_cmd[cmd].insert(t);
    };

    for (const auto& root : roots) {
        fs::path rp(root);
        if (!fs::exists(rp)) continue;

        for (auto it = fs::recursive_directory_iterator(rp);
             it != fs::recursive_directory_iterator();
             ++it) {
            if (!it->is_regular_file()) continue;

            const auto& p = it->path();
            const auto ext = up(p.extension().string());
            if (ext != ".CPP" && ext != ".HPP" && ext != ".H" && ext != ".CC" && ext != ".CXX") continue;

            std::ifstream fin(p, std::ios::in | std::ios::binary);
            if (!fin) continue;
            std::string s((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());

            std::string cmd = {};
            std::smatch mreg;
            if (std::regex_search(s, mreg, rx_reg) && mreg.size() > 1) cmd = up(mreg[1].str());
            if (cmd.empty()) cmd = command_hint_from_path(p);
            if (cmd.empty()) continue;

            for (auto it2 = std::sregex_iterator(s.begin(), s.end(), rx_case_token);
                 it2 != std::sregex_iterator(); ++it2) consider(cmd, (*it2)[1].str());

            for (auto it2 = std::sregex_iterator(s.begin(), s.end(), rx_token);
                 it2 != std::sregex_iterator(); ++it2) consider(cmd, (*it2)[1].str());

            for (auto it2 = std::sregex_iterator(s.begin(), s.end(), rx_has);
                 it2 != std::sregex_iterator(); ++it2) {
                if ((*it2)[1].matched) consider(cmd, (*it2)[1].str());
                else if ((*it2)[2].matched) consider(cmd, (*it2)[2].str());
            }

            for (auto it2 = std::sregex_iterator(s.begin(), s.end(), rx_str);
                 it2 != std::sregex_iterator(); ++it2) consider(cmd, (*it2)[1].str());

            for (auto it2 = std::sregex_iterator(s.begin(), s.end(), rx_set);
                 it2 != std::sregex_iterator(); ++it2) {
                consider(cmd, (*it2)[1].str());
                consider(cmd, (*it2)[2].str());
            }

            for (auto it2 = std::sregex_iterator(s.begin(), s.end(), rx_no);
                 it2 != std::sregex_iterator(); ++it2) consider(cmd, "NO" + (*it2)[1].str());
        }
    }

    return per_cmd;
}

// ==== minimal DBF/DBT writer =================================================

#pragma pack(push,1)
struct DbfHeader {
    uint8_t  version{0x03};
    uint8_t  y{0}, m{0}, d{0};
    uint32_t nrecs{0};
    uint16_t header_len{0};
    uint16_t rec_len{0};
    uint8_t  reserved[20]{};
};

struct DbfField {
    char     name[11]{};
    char     type{'C'};
    uint32_t data_addr{0};
    uint8_t  length{0};
    uint8_t  decimals{0};
    uint8_t  reserved[14]{};
};
#pragma pack(pop)

static void put_name(char dst[11], const std::string& s) {
    std::memset(dst, 0, 11);
    std::strncpy(dst, s.c_str(), 10);
}

static DbfField fieldC(const std::string& name, uint8_t len) {
    DbfField f{};
    put_name(f.name, name);
    f.type = 'C';
    f.length = len;
    return f;
}

static DbfField fieldL(const std::string& name) {
    DbfField f{};
    put_name(f.name, name);
    f.type = 'L';
    f.length = 1;
    return f;
}

static DbfField fieldN(const std::string& name, uint8_t len) {
    DbfField f{};
    put_name(f.name, name);
    f.type = 'N';
    f.length = len;
    f.decimals = 0;
    return f;
}

static DbfField fieldM(const std::string& name) {
    DbfField f{};
    put_name(f.name, name);
    f.type = 'M';
    f.length = 10;
    return f;
}

static uint16_t u16_clamp_size(size_t v) {
    return static_cast<uint16_t>(std::min<size_t>(v, 65535));
}

class DBTWriter {
public:
    explicit DBTWriter(const std::string& path)
        : path_(path), blockSize_(512), nextBlock_(1)
    {
        out_.open(path_, std::ios::binary | std::ios::trunc);
        if (!out_) throw std::runtime_error("cannot open " + path_);
        write_header();
    }

    uint32_t append(const std::string& text) {
        const uint32_t start = nextBlock_;

        std::string payload;
        payload.resize(4);

        const uint32_t len = static_cast<uint32_t>(text.size());
        payload[0] = static_cast<char>((len >>  0) & 0xFF);
        payload[1] = static_cast<char>((len >>  8) & 0xFF);
        payload[2] = static_cast<char>((len >> 16) & 0xFF);
        payload[3] = static_cast<char>((len >> 24) & 0xFF);

        payload += text;
        payload.push_back(0x1A);

        size_t total = payload.size();
        size_t blocks = (total + blockSize_ - 1) / blockSize_;
        payload.resize(blocks * blockSize_, 0x00);

        out_.seekp(static_cast<std::streamoff>(start) * static_cast<std::streamoff>(blockSize_), std::ios::beg);
        out_.write(payload.data(), static_cast<std::streamsize>(payload.size()));

        nextBlock_ += static_cast<uint32_t>(blocks);
        flush_header();
        return start;
    }

    void close() {
        if (out_.is_open()) {
            flush_header();
            out_.flush();
            out_.close();
        }
    }

    ~DBTWriter() { close(); }

private:
    void write_header() {
        std::string hdr(blockSize_, 0x00);
        hdr[0] = static_cast<char>((nextBlock_ >>  0) & 0xFF);
        hdr[1] = static_cast<char>((nextBlock_ >>  8) & 0xFF);
        hdr[2] = static_cast<char>((nextBlock_ >> 16) & 0xFF);
        hdr[3] = static_cast<char>((nextBlock_ >> 24) & 0xFF);
        out_.seekp(0, std::ios::beg);
        out_.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
    }

    void flush_header() {
        std::string hdr(blockSize_, 0x00);
        hdr[0] = static_cast<char>((nextBlock_ >>  0) & 0xFF);
        hdr[1] = static_cast<char>((nextBlock_ >>  8) & 0xFF);
        hdr[2] = static_cast<char>((nextBlock_ >> 16) & 0xFF);
        hdr[3] = static_cast<char>((nextBlock_ >> 24) & 0xFF);
        out_.seekp(0, std::ios::beg);
        out_.write(hdr.data(), static_cast<std::streamsize>(hdr.size()));
    }

    std::string   path_;
    std::ofstream out_;
    const uint32_t blockSize_;
    uint32_t nextBlock_;
};

static void write_dbf_with_memo(const std::string& dbf_path,
                                const std::string& dbt_path,
                                const std::vector<DbfField>& fields,
                                const std::vector<std::vector<std::string>>& rows)
{
    DBTWriter dbt(dbt_path);

    const uint16_t header_len = u16_clamp_size(sizeof(DbfHeader) + (fields.size() * sizeof(DbfField)) + 1);
    uint16_t rec_len = 1;
    for (auto& f : fields) rec_len = static_cast<uint16_t>(rec_len + f.length);

    auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    std::chrono::year_month_day ymd{today};

    DbfHeader hdr{};
    hdr.y = static_cast<uint8_t>(int(ymd.year()) % 100);
    hdr.m = static_cast<uint8_t>(unsigned(ymd.month()));
    hdr.d = static_cast<uint8_t>(unsigned(ymd.day()));
    hdr.nrecs = static_cast<uint32_t>(rows.size());
    hdr.header_len = header_len;
    hdr.rec_len = rec_len;

    std::ofstream out(dbf_path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot open " + dbf_path);

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    for (auto f : fields) out.write(reinterpret_cast<const char*>(&f), sizeof(f));
    const char term = 0x0D;
    out.write(&term, 1);

    auto write_fixed = [&](const std::string& s, uint8_t len) {
        std::string buf(len, ' ');
        std::memcpy(&buf[0], s.c_str(), std::min<size_t>(len, s.size()));
        out.write(buf.data(), len);
    };

    auto write_memo_ptr = [&](uint32_t block) {
        std::string ptr = std::to_string(block);
        if (ptr.size() < 10) ptr.append(10 - ptr.size(), ' ');
        else if (ptr.size() > 10) ptr = ptr.substr(0, 10);
        out.write(ptr.data(), 10);
    };

    for (const auto& r : rows) {
        const char not_deleted = ' ';
        out.write(&not_deleted, 1);

        for (size_t i = 0; i < fields.size(); ++i) {
            const auto& f = fields[i];
            std::string cell = (i < r.size() ? r[i] : "");

            switch (f.type) {
                case 'L': {
                    const char v = (!cell.empty() && (cell[0] == 'Y' || cell == "1" ||
                                                      cell[0] == 'y' || cell[0] == 'T' || cell[0] == 't')) ? 'T' : 'F';
                    std::string tmp(1, v);
                    write_fixed(tmp, f.length);
                    break;
                }
                case 'N': {
                    std::string buf = cell;
                    if (buf.size() < f.length) buf = std::string(f.length - buf.size(), ' ') + buf;
                    else if (buf.size() > f.length) buf = buf.substr(buf.size() - f.length);
                    out.write(buf.data(), f.length);
                    break;
                }
                case 'M': {
                    uint32_t block = 0;
                    if (!cell.empty()) block = dbt.append(cell);
                    write_memo_ptr(block);
                    break;
                }
                default: {
                    if (cell.size() < f.length) cell.append(f.length - cell.size(), ' ');
                    else if (cell.size() > f.length) cell = cell.substr(0, f.length);
                    out.write(cell.data(), f.length);
                    break;
                }
            }
        }
    }

    const char eof = 0x1A;
    out.write(&eof, 1);
}

// ==== minimal DBF/DBT reader (for SHOW mode) =================================

class DBTReader {
public:
    explicit DBTReader(const std::string& path)
        : path_(path), blockSize_(512)
    {
        in_.open(path_, std::ios::binary);
        valid_ = in_.good();
    }

    bool valid() const { return valid_; }

    std::string read_block_string(uint32_t start_block) {
        if (!valid_ || start_block == 0) return {};

        const std::streamoff off =
            static_cast<std::streamoff>(start_block) * static_cast<std::streamoff>(blockSize_);

        in_.seekg(off, std::ios::beg);

        char lenb[4]{};
        in_.read(lenb, 4);
        if (!in_) return {};

        uint32_t len = (static_cast<unsigned char>(lenb[0])      ) |
                       (static_cast<unsigned char>(lenb[1]) <<  8) |
                       (static_cast<unsigned char>(lenb[2]) << 16) |
                       (static_cast<unsigned char>(lenb[3]) << 24);

        std::string s(len, '\0');
        in_.read(&s[0], static_cast<std::streamsize>(len));
        return s;
    }

private:
    std::string   path_;
    std::ifstream in_;
    const uint32_t blockSize_;
    bool          valid_{false};
};

struct DbfTable {
    std::vector<DbfField> fields;
    std::vector<std::vector<std::string>> rows;
};

static bool read_dbf_with_memo(const std::string& dbf_path,
                               const std::string& dbt_path,
                               DbfTable& out)
{
    std::ifstream in(dbf_path, std::ios::binary);
    if (!in) return false;

    DbfHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;

    const int nfields = (hdr.header_len - sizeof(DbfHeader) - 1) / sizeof(DbfField);
    if (nfields <= 0) return false;

    out.fields.resize(nfields);
    for (int i = 0; i < nfields; ++i) {
        in.read(reinterpret_cast<char*>(&out.fields[i]), sizeof(DbfField));
    }

    char term{};
    in.read(&term, 1);

    DBTReader dbt(dbt_path);

    auto read_fixed = [&](uint8_t len) -> std::string {
        std::string buf(len, '\0');
        in.read(&buf[0], len);
        if (!in) return {};
        while (!buf.empty() && buf.back() == ' ') buf.pop_back();
        return buf;
    };

    out.rows.clear();
    out.rows.reserve(hdr.nrecs);

    for (uint32_t r = 0; r < hdr.nrecs; ++r) {
        char delFlag = 0;
        in.read(&delFlag, 1);

        std::vector<std::string> row;
        row.reserve(nfields);

        for (int i = 0; i < nfields; ++i) {
            const auto& f = out.fields[i];
            if (f.type == 'M') {
                std::string ptr = read_fixed(10);
                uint32_t blk = 0;
                if (!ptr.empty()) {
                    try { blk = static_cast<uint32_t>(std::stoul(ptr)); }
                    catch (...) { blk = 0; }
                }
                row.push_back(dbt.read_block_string(blk));
            } else {
                row.push_back(read_fixed(f.length));
            }
        }

        out.rows.push_back(std::move(row));
    }

    return true;
}

// ==== end utils ==============================================================

} // namespace

namespace cmdhelp {

// === catalog & args ==========================================================

std::vector<CommandInfo> collect_commands() {
    std::unordered_set<std::string> implemented;
    for (const auto& kv : dli::map()) implemented.insert(up(kv.first));

    std::vector<CommandInfo> out;
    out.reserve(foxref::catalog().size() + dotref::catalog().size() + edref::catalog().size() + implemented.size());
    int next_id = 1;

    std::unordered_set<std::string> seen_keys;

    auto add_catalog_item = [&](const std::string& catalog_name,
                                const std::string& name,
                                bool supported,
                                const std::string& usage,
                                const std::string& verbose)
    {
        const std::string key = catalog_name + "|" + name;
        if (!seen_keys.insert(key).second) return;

        CommandInfo ci;
        ci.id = next_id++;
        ci.catalog = catalog_name;
        ci.name = name;
        ci.implemented = implemented.count(name) > 0;
        ci.supported = supported;
        ci.usage = usage;
        ci.verbose = verbose;
        out.push_back(std::move(ci));
    };

    for (const auto& it : foxref::catalog()) {
        add_catalog_item("FOX", up(it.name), it.supported,
                         to_string_or_empty(it.syntax),
                         to_string_or_empty(it.summary));
    }

    for (const auto& it : dotref::catalog()) {
        add_catalog_item("DOT", up(it.name), it.supported,
                         to_string_or_empty(it.syntax),
                         to_string_or_empty(it.summary));
    }

    for (const auto& it : edref::catalog()) {
        add_catalog_item("ED", up(it.topic), it.supported,
                         to_string_or_empty(it.syntax),
                         to_string_or_empty(it.summary));
    }

    for (const auto& key : implemented) {
        bool seen_any = false;
        if (seen_keys.count("FOX|" + key)) seen_any = true;
        if (seen_keys.count("DOT|" + key)) seen_any = true;
        if (seen_keys.count("ED|"  + key)) seen_any = true;

        if (!seen_any) {
            CommandInfo ci;
            ci.id = next_id++;
            ci.catalog = "DOT";
            ci.name = key;
            ci.implemented = true;
            ci.supported = false;
            ci.usage.clear();
            ci.verbose = "Homegrown command.";
            out.push_back(std::move(ci));
        }
    }

    std::sort(out.begin(), out.end(), [](const CommandInfo& a, const CommandInfo& b) {
        if (a.catalog != b.catalog) return a.catalog < b.catalog;
        if (a.implemented != b.implemented) return a.implemented > b.implemented;
        return a.name < b.name;
    });

    return out;
}

std::vector<ArgInfo> collect_args(const std::vector<CommandInfo>& cmds,
                                  const std::vector<std::string>& source_roots)
{
    std::unordered_map<std::string, const foxref::Item*> fox_by_name;
    for (const auto& it : foxref::catalog()) fox_by_name.emplace(up(it.name), &it);

    std::unordered_map<std::string, const dotref::Item*> dot_by_name;
    for (const auto& it : dotref::catalog()) dot_by_name.emplace(up(it.name), &it);

    std::unordered_map<std::string, const edref::Item*> ed_by_name;
    for (const auto& it : edref::catalog()) ed_by_name.emplace(up(it.topic), &it);

    std::unordered_map<std::string, std::set<std::string>> per_cmd;

    for (const auto& c : cmds) {
        std::string key = c.catalog + "|" + c.name;

        if (c.catalog == "FOX") {
            auto it = fox_by_name.find(c.name);
            if (it != fox_by_name.end() && it->second->syntax) {
                for (const auto& sw : switches_from_syntax(it->second->syntax)) {
                    per_cmd[key].insert(sw);
                }
            }
        } else if (c.catalog == "DOT") {
            auto it = dot_by_name.find(c.name);
            if (it != dot_by_name.end() && it->second->syntax) {
                for (const auto& sw : switches_from_syntax(it->second->syntax)) {
                    per_cmd[key].insert(sw);
                }
            }
        } else if (c.catalog == "ED") {
            auto it = ed_by_name.find(c.name);
            if (it != ed_by_name.end() && it->second->syntax) {
                for (const auto& sw : switches_from_syntax(it->second->syntax)) {
                    per_cmd[key].insert(sw);
                }
            }
        }

        for (auto k : {"FOR","WHILE","NEXT","RECORD","REST"}) {
            per_cmd[key].insert(k);
        }
    }

    auto mined = scan_sources_for_switches(source_roots);
    for (const auto& c : cmds) {
        auto mit = mined.find(c.name);
        if (mit == mined.end()) continue;
        auto& dst = per_cmd[c.catalog + "|" + c.name];
        dst.insert(mit->second.begin(), mit->second.end());
    }

    std::vector<ArgInfo> out;
    int next_id = 1;

    for (const auto& c : cmds) {
        const std::string key = c.catalog + "|" + c.name;
        const auto it = per_cmd.find(key);
        if (it == per_cmd.end()) continue;

        std::string usage_seed;
        std::string verb_seed;

        if (c.catalog == "FOX") {
            auto fit = fox_by_name.find(c.name);
            if (fit != fox_by_name.end()) {
                usage_seed = fit->second->syntax ? fit->second->syntax : "";
                verb_seed  = fit->second->summary ? fit->second->summary : "";
            }
        } else if (c.catalog == "DOT") {
            auto dit = dot_by_name.find(c.name);
            if (dit != dot_by_name.end()) {
                usage_seed = dit->second->syntax ? dit->second->syntax : "";
                verb_seed  = dit->second->summary ? dit->second->summary : "";
            }
        } else if (c.catalog == "ED") {
            auto eit = ed_by_name.find(c.name);
            if (eit != ed_by_name.end()) {
                usage_seed = eit->second->syntax ? eit->second->syntax : "";
                verb_seed  = eit->second->summary ? eit->second->summary : "";
            }
        }

        for (const auto& sw : it->second) {
            ArgInfo ai;
            ai.id = next_id++;
            ai.catalog = c.catalog;
            ai.command = c.name;
            ai.arg = sw;
            ai.usage = usage_seed;
            ai.verbose = verb_seed;
            out.push_back(std::move(ai));
        }
    }

    std::sort(out.begin(), out.end(), [](const ArgInfo& a, const ArgInfo& b) {
        if (a.catalog != b.catalog) return a.catalog < b.catalog;
        if (a.command != b.command) return a.command < b.command;
        return a.arg < b.arg;
    });

    return out;
}

// === reporting ==============================================================

void print_commands_report(std::ostream& os, const std::vector<CommandInfo>& cmds) {
    os << "Commands (registry U foxref U dotref U edref)\n";
    os << "-----------------------------------------------------------------------\n";
    os << std::left << std::setw(4)  << "ID"
       << " " << std::setw(8)  << "CAT"
       << " " << std::setw(18) << "NAME"
       << " " << std::setw(12) << "IMPLEMENTED"
       << " " << std::setw(10) << "SUPPORTED"
       << " " << "SUMMARY/USAGE\n";

    for (const auto& c : cmds) {
        os << std::left << std::setw(4)  << c.id
           << " " << std::setw(8)  << c.catalog
           << " " << std::setw(18) << c.name
           << " " << std::setw(12) << (c.implemented ? "yes" : "no")
           << " " << std::setw(10) << (c.supported ? "yes" : "no")
           << " " << (c.verbose.empty() ? c.usage : c.verbose)
           << "\n";
    }
}

// === DBF export with memo ====================================================

DbfWriteCounts export_dbfs(const std::string& out_dir,
                           const std::vector<std::string>& source_roots)
{
    const auto cmds = collect_commands();
    const auto args = collect_args(cmds, source_roots);

    fs::create_directories(out_dir);

    std::vector<DbfField> f_cmd = {
        fieldN("ID",        10),
        fieldC("CATALOG",   8),
        fieldC("COMMAND",   24),
        fieldC("CMDKEY",    40),
        fieldL("IMPLEMENT"),
        fieldL("SUPPORTED"),
        fieldM("USAGE"),
        fieldM("VERBOSE"),
    };

    std::vector<std::vector<std::string>> rows_cmd;
    rows_cmd.reserve(cmds.size());

    for (const auto& c : cmds) {
        rows_cmd.push_back({
            std::to_string(c.id),
            c.catalog,
            c.name,
            make_cmdkey(c.catalog, c.name),
            c.implemented ? "Y" : "N",
            c.supported ? "Y" : "N",
            c.usage,
            c.verbose
        });
    }

    write_dbf_with_memo((fs::path(out_dir) / "commands.dbf").string(),
                        (fs::path(out_dir) / "commands.dbt").string(),
                        f_cmd, rows_cmd);

    std::vector<DbfField> f_arg = {
        fieldN("ID",        10),
        fieldC("CATALOG",   8),
        fieldC("COMMAND",   24),
        fieldC("CMDKEY",    40),
        fieldC("ARG",       24),
        fieldM("USAGE"),
        fieldM("VERBOSE"),
    };

    std::vector<std::vector<std::string>> rows_arg;
    rows_arg.reserve(args.size());

    for (const auto& a : args) {
        rows_arg.push_back({
            std::to_string(a.id),
            a.catalog,
            a.command,
            make_cmdkey(a.catalog, a.command),
            a.arg,
            a.usage,
            a.verbose
        });
    }

    write_dbf_with_memo((fs::path(out_dir) / "cmd_args.dbf").string(),
                        (fs::path(out_dir) / "cmd_args.dbt").string(),
                        f_arg, rows_arg);

    return { static_cast<int>(rows_cmd.size()), static_cast<int>(rows_arg.size()) };
}

// === SHOW (read existing DBFs) ===============================================

class CmdDbfLoader {
public:
    static bool load(const std::string& dir, std::vector<CommandInfo>& out_cmds, int& out_arg_rows) {
        DbfTable tbl_cmd;
        const std::string dbf = (fs::path(dir) / "commands.dbf").string();
        const std::string dbt = (fs::path(dir) / "commands.dbt").string();
        if (!read_dbf_with_memo(dbf, dbt, tbl_cmd)) return false;

        auto idx_of = [&](const char* name) -> int {
            std::string U = up(name);
            for (size_t i = 0; i < tbl_cmd.fields.size(); ++i) {
                std::string fn(tbl_cmd.fields[i].name,
                               tbl_cmd.fields[i].name + strnlen(tbl_cmd.fields[i].name, 11));
                if (up(fn) == U) return static_cast<int>(i);
            }
            return -1;
        };

        const int ixID   = idx_of("ID");
        const int ixCAT  = idx_of("CATALOG");
        const int ixCMD  = idx_of("COMMAND");
        const int ixKEY  = idx_of("CMDKEY");
        const int ixIMPL = idx_of("IMPLEMENT");
        const int ixSUP  = idx_of("SUPPORTED");
        const int ixUSE  = idx_of("USAGE");
        const int ixVER  = idx_of("VERBOSE");
        (void)ixKEY;

        if (ixID < 0 || ixCAT < 0 || ixCMD < 0 || ixIMPL < 0 || ixSUP < 0 || ixUSE < 0 || ixVER < 0) {
            return false;
        }

        out_cmds.clear();
        out_cmds.reserve(tbl_cmd.rows.size());

        for (const auto& r : tbl_cmd.rows) {
            CommandInfo ci;
            try { ci.id = std::stoi(r[ixID]); }
            catch (...) { ci.id = 0; }

            ci.catalog = up(r[ixCAT]);
            ci.name = up(r[ixCMD]);

            auto impl = r[ixIMPL];
            ci.implemented = (!impl.empty() && (impl[0] == 'T' || impl[0] == 't' || impl[0] == 'Y' || impl == "1"));

            auto supp = r[ixSUP];
            ci.supported = (!supp.empty() && (supp[0] == 'T' || supp[0] == 't' || supp[0] == 'Y' || supp == "1"));

            ci.usage   = r[ixUSE];
            ci.verbose = r[ixVER];
            out_cmds.push_back(std::move(ci));
        }

        DbfTable tbl_args;
        const std::string adb = (fs::path(dir) / "cmd_args.dbf").string();
        const std::string adt = (fs::path(dir) / "cmd_args.dbt").string();
        out_arg_rows = read_dbf_with_memo(adb, adt, tbl_args) ? static_cast<int>(tbl_args.rows.size()) : 0;

        std::sort(out_cmds.begin(), out_cmds.end(), [](const CommandInfo& a, const CommandInfo& b) {
            if (a.catalog != b.catalog) return a.catalog < b.catalog;
            if (a.implemented != b.implemented) return a.implemented > b.implemented;
            return a.name < b.name;
        });

        return true;
    }
};

// === CLI =====================================================================

void cmd_COMMANDSHELP(DbArea& /*area*/, std::istringstream& in) {
    auto trim_inplace = [](std::string& s) {
        auto issp = [](unsigned char c){ return std::isspace(c) != 0; };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c){ return !issp(c); }));
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    };

    std::string first;
    {
        std::streampos pos = in.tellg();
        in >> first;
        if (!in) {
            in.clear();
            in.seekg(pos);
        }
    }

    std::string firstU = up(first);

    if (firstU == "BUILD") {
        std::string dummy;
        std::getline(in, dummy);

        std::string outdir;
        std::getline(in, outdir);
        trim_inplace(outdir);
        fs::path outdir_p = resolve_help_dir_arg(outdir);
        outdir = outdir_p.string();

        std::string srcroot;
        std::getline(in, srcroot);
        trim_inplace(srcroot);

        std::vector<std::string> roots =
            srcroot.empty() ? std::vector<std::string>{"./src"} : std::vector<std::string>{srcroot};

        auto counts = export_dbfs(outdir, roots);
        auto list = collect_commands();

        std::cout << "CMDHELP wrote: " << counts.commands << " command rows, "
                  << counts.args << " arg rows -> " << outdir << "\n"
                  << "Switches mined from: " << (srcroot.empty() ? "./src" : srcroot) << "\n\n";
        print_commands_report(std::cout, list);
        return;
    }

    in.clear();
    std::string line;
    std::getline(in, line);
    trim_inplace(line);

    fs::path dir_p = resolve_help_dir_arg(line);
    std::string dir = dir_p.string();

    std::vector<CommandInfo> cmds;
    int arg_rows = 0;

    if (!CmdDbfLoader::load(dir, cmds, arg_rows)) {
        std::cout << "CMDHELP: could not read existing DBFs in \"" << dir << "\".\n"
                  << "Tip: run:  CMDHELP BUILD\n";
        return;
    }

    int cmd_rows = static_cast<int>(cmds.size());

    std::cout << "CMDHELP Report (last CMDHELP BUILD): "
              << cmd_rows << " command rows, "
              << arg_rows << " arg rows -> " << dir << "\n"
              << "Switches mined from: (existing DBFs)\n\n";
    print_commands_report(std::cout, cmds);
}

} // namespace cmdhelp

void cmd_CMDHELP(DbArea& area, std::istringstream& in) {
    cmdhelp::cmd_COMMANDSHELP(area, in);
}
