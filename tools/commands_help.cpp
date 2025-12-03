// ============================================================================
// /src/cli/commands_help.cpp
// ============================================================================

#include "commands_help.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <unordered_set>

#include "xbase.hpp" // xbase::DbArea (only for CLI signature)
using xbase::DbArea;

namespace fs = std::filesystem;

namespace {

std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

// Minimal token extraction from a foxref syntax line, yielding candidate arguments/switches.
// WHY: We avoid confusing arguments (e.g., <table>) with switches (ALL, VERBOSE, TAG, INDEX, NOINDEX).
std::vector<std::string> switches_from_syntax(const std::string& syntax) {
    static const std::unordered_set<std::string> IGN = {
        "USE","FIELDS","INDEX","SETINDEX","LIST","FIND","SEEK","GOTO","TOP","BOTTOM","APPEND","APPEND BLANK",
        "REPLACE","DELETE","RECALL","PACK","EXPORT","IMPORT","STRUCT","STATUS","COUNT",
        "SET","ORDER","TO","<TABLE>","<FLD>","<FIELD>","<VALUE>","<CSV>","<N>","<TAG>","<EXPR>","#N",
        "<SPEC>","<NAME>","<RECNO>","<VAL>","<FIELD|#N>","<TAG|PATH>","<FLD>","<OP>","<AREA>","INTO","IN"
    };
    static const std::unordered_set<std::string> LIKELY_SWITCHES = {
        "ALL","VERBOSE","TAG","INDEX","NOINDEX","FOR","WHILE","NEXT","RECORD","REST","ASC","DESC","UNIQUE","FIELDS","ALIAS"
    };

    std::vector<std::string> out;
    std::string t;
    for (char c : syntax) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c=='_' ) { t.push_back(std::toupper(c)); }
        else {
            if (!t.empty()) {
                if (!IGN.count(t) && LIKELY_SWITCHES.count(t)) out.push_back(t);
                t.clear();
            }
        }
    }
    if (!t.empty()) {
        if (!IGN.count(t) && LIKELY_SWITCHES.count(t)) out.push_back(t);
    }
    // De-dupe
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

} // namespace

namespace cmdhelp {

std::vector<CommandInfo> collect_commands() {
    // Registry → implemented handlers
    std::unordered_set<std::string> implemented;
    for (const auto& kv : dli::map()) {
        implemented.insert(up(kv.first));
    }

    // Merge with foxref catalog
    std::vector<CommandInfo> out;
    out.reserve(foxref::catalog().size());
    int next_id = 1;

    // Index foxref items by name
    std::unordered_map<std::string, const foxref::Item*> fox_by_name;
    for (const auto& it : foxref::catalog()) {
        fox_by_name.emplace(up(it.name), &it);
    }

    // Primary pass: every foxref item
    for (const auto& it : foxref::catalog()) {
        const std::string name = up(it.name);
        CommandInfo ci;
        ci.id = next_id++;
        ci.name = name;
        ci.implemented = implemented.count(name) > 0;
        ci.foxref_supported = it.supported;
        ci.usage   = it.syntax ? it.syntax : "";
        ci.verbose = it.summary ? it.summary : "";
        out.push_back(std::move(ci));
    }

    // Secondary pass: any implemented command not in foxref (homegrown)
    for (const auto& key : implemented) {
        if (!fox_by_name.count(key)) {
            CommandInfo ci;
            ci.id = next_id++;
            ci.name = key;
            ci.implemented = true;
            ci.foxref_supported = false;
            ci.usage = "";   // can be filled later
            ci.verbose = "Homegrown command.";
            out.push_back(std::move(ci));
        }
    }

    // Stable order: implemented first, then name
    std::sort(out.begin(), out.end(), [](const CommandInfo& a, const CommandInfo& b){
        if (a.implemented != b.implemented) return a.implemented > b.implemented;
        return a.name < b.name;
    });
    return out;
}

std::vector<ArgInfo> collect_args(const std::vector<CommandInfo>& cmds) {
    std::unordered_map<std::string, const foxref::Item*> fox_by_name;
    for (const auto& it : foxref::catalog()) {
        fox_by_name.emplace(up(it.name), &it);
    }

    std::vector<ArgInfo> out;
    int next_id = 1;
    std::set<std::pair<std::string,std::string>> seen;

    for (const auto& c : cmds) {
        auto it = fox_by_name.find(c.name);
        if (it == fox_by_name.end()) continue;
        const auto* item = it->second;
        if (!item->syntax) continue;

        for (const auto& sw : switches_from_syntax(item->syntax)) {
            auto key = std::make_pair(c.name, sw);
            if (seen.insert(key).second) {
                ArgInfo ai;
                ai.id = next_id++;
                ai.command = c.name;
                ai.arg = sw;
                ai.usage = item->syntax;
                ai.verbose = (item->summary ? item->summary : std::string{});
                out.push_back(std::move(ai));
            }
        }
    }

    // Order: by command, then arg
    std::sort(out.begin(), out.end(), [](const ArgInfo& a, const ArgInfo& b){
        if (a.command != b.command) return a.command < b.command;
        return a.arg < b.arg;
    });
    return out;
}

void print_commands_report(std::ostream& os, const std::vector<CommandInfo>& cmds) {
    os << "Commands (merged registry + foxref)\n";
    os << "-------------------------------------------------------------\n";
    os << std::left << std::setw(4) << "ID"
       << " " << std::setw(14) << "NAME"
       << " " << std::setw(12) << "IMPLEMENTED"
       << " " << std::setw(10) << "FOXREF"
       << " " << "SUMMARY/USAGE\n";
    for (const auto& c : cmds) {
        os << std::left << std::setw(4) << c.id
           << " " << std::setw(14) << c.name
           << " " << std::setw(12) << (c.implemented ? "yes" : "no")
           << " " << std::setw(10) << (c.foxref_supported ? "yes" : "no")
           << " " << (c.verbose.empty() ? c.usage : c.verbose)
           << "\n";
    }
}

// --- Tiny DBF writer (C/L/N only). WHY: Memo M will land later; we keep USAGE/VERBOSE in C(254) now.

#pragma pack(push,1)
struct DbfHeader {
    uint8_t  version{0x03};   // dBASE III without memo
    uint8_t  y{0}, m{0}, d{0};
    uint32_t nrecs{0};
    uint16_t header_len{0};
    uint16_t rec_len{0};
    uint8_t  reserved[20]{};
};

struct DbfField {
    char     name[11]{};
    char     type{'C'};       // 'C','L','N'
    uint32_t data_addr{0};    // unused for dBASE III
    uint8_t  length{0};
    uint8_t  decimals{0};
    uint8_t  reserved[14]{};
};
#pragma pack(pop)

static void put_name(char dst[11], const std::string& s) {
    std::memset(dst, 0, 11);
    std::strncpy(dst, s.c_str(), 10);
}

static void write_dbf(const std::string& path,
                      const std::vector<DbfField>& fields,
                      const std::vector<std::vector<std::string>>& rows)
{
    // Compute sizes
    const uint16_t header_len = sizeof(DbfHeader) + (fields.size() * sizeof(DbfField)) + 1; // +1 for 0x0D
    uint16_t rec_len = 1; // deletion flag
    for (auto& f : fields) rec_len += f.length;

    // Header
    DbfHeader hdr{};
    // Simplified date: 2025-11-19 (hardcoded example – adjust if needed)
    hdr.y = 25; hdr.m = 11; hdr.d = 19;
    hdr.nrecs = static_cast<uint32_t>(rows.size());
    hdr.header_len = header_len;
    hdr.rec_len = rec_len;

    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot open " + path);
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    for (auto f : fields) out.write(reinterpret_cast<const char*>(&f), sizeof(f));
    const char term = 0x0D;
    out.write(&term, 1);

    // Records
    auto write_padded = [&](const std::string& s, uint8_t len){
        std::string buf(len, ' ');
        if (s.size() <= len) {
            std::memcpy(&buf[0], s.c_str(), s.size());
        } else {
            std::memcpy(&buf[0], s.c_str(), len);
        }
        out.write(buf.data(), len);
    };

    for (const auto& r : rows) {
        const char not_deleted = ' ';
        out.write(&not_deleted, 1);
        for (size_t i = 0; i < fields.size(); ++i) {
            const auto& f = fields[i];
            std::string cell = (i < r.size() ? r[i] : "");
            if (f.type == 'L') {
                cell = (!cell.empty() && (cell[0]=='Y' || cell[0]=='y' || cell=="1")) ? "T" : "F";
                write_padded(cell, f.length);
            } else if (f.type == 'N') {
                // right-align numeric
                std::string buf = cell;
                if (buf.size() < f.length) buf = std::string(f.length - buf.size(), ' ') + buf;
                else if (buf.size() > f.length) buf = buf.substr(buf.size() - f.length);
                out.write(buf.data(), f.length);
            } else {
                write_padded(cell, f.length);
            }
        }
    }
    const char eof = 0x1A;
    out.write(&eof, 1);
}

static DbfField fieldC(const std::string& name, uint8_t len) {
    DbfField f{}; put_name(f.name, name); f.type='C'; f.length=len; return f;
}
static DbfField fieldL(const std::string& name) {
    DbfField f{}; put_name(f.name, name); f.type='L'; f.length=1; return f;
}
static DbfField fieldN(const std::string& name, uint8_t len) {
    DbfField f{}; put_name(f.name, name); f.type='N'; f.length=len; f.decimals=0; return f;
}

DbfWriteCounts export_dbfs(const std::string& out_dir) {
    const auto cmds = collect_commands();
    const auto args = collect_args(cmds);

    fs::create_directories(out_dir);

    // commands.dbf
    std::vector<DbfField> f_cmd = {
        fieldN("ID",        10),
        fieldC("COMMAND",   24),
        fieldL("IMPLEMENT"),       // IMPLEMENTED
        fieldL("SUPPORTED"),       // FOXREF supported
        fieldC("USAGE",     254),
        fieldC("VERBOSE",   254),
    };
    std::vector<std::vector<std::string>> rows_cmd;
    rows_cmd.reserve(cmds.size());
    for (const auto& c : cmds) {
        rows_cmd.push_back({
            std::to_string(c.id),
            c.name,
            c.implemented ? "Y" : "N",
            c.foxref_supported ? "Y" : "N",
            c.usage,
            c.verbose
        });
    }
    write_dbf((fs::path(out_dir)/"commands.dbf").string(), f_cmd, rows_cmd);

    // cmd_args.dbf
    std::vector<DbfField> f_arg = {
        fieldN("ID",        10),
        fieldC("COMMAND",   24),
        fieldC("ARG",       24),
        fieldC("USAGE",     254),
        fieldC("VERBOSE",   254),
    };
    std::vector<std::vector<std::string>> rows_arg;
    rows_arg.reserve(args.size());
    for (const auto& a : args) {
        rows_arg.push_back({
            std::to_string(a.id),
            a.command,
            a.arg,
            a.usage,
            a.verbose
        });
    }
    write_dbf((fs::path(out_dir)/"cmd_args.dbf").string(), f_arg, rows_arg);

    return { static_cast<int>(rows_cmd.size()), static_cast<int>(rows_arg.size()) };
}

void cmd_COMMANDSHELP(DbArea& /*area*/, std::istringstream& in) {
    std::string outdir;
    std::getline(in, outdir);
    if (outdir.empty()) outdir = ".";
    outdir.erase(outdir.begin(), std::find_if(outdir.begin(), outdir.end(), [](unsigned char c){ return !std::isspace(c); }));

    auto counts = export_dbfs(outdir);
    auto cmds = collect_commands();

    std::cout << "COMMANDSHELP wrote: " << counts.commands << " command rows and "
              << counts.args << " arg rows into: " << outdir << "\n\n";
    print_commands_report(std::cout, cmds);
}

} // namespace cmdhelp

// Register handler (optional): if you wire this in your shell, e.g.:
//   dli::register_command("COMMANDSHELP", [](DbArea& A, std::istringstream& in){ cmdhelp::cmd_COMMANDSHELP(A,in); });
