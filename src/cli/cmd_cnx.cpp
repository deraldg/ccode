// src/cli/cmd_cnx.cpp — CNX utility command (CREATE/INFO/TAGS/ADDTAG/DROPTAG)
// Policy enforced:
//   • CREATE: refuse if file exists.
//   • INFO/TAGS/ADDTAG/DROPTAG: require existing file (no implicit creation).
//   • Default path (no arg): <current_dbf_basename>.cnx in same folder.

#include <sstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <cctype>
#include <memory>
#include <system_error>

#include "xbase.hpp"
#include "cnx/cnx.hpp"

namespace fs = std::filesystem;
using cnxfile::CNXHandle;

static inline std::string trim_copy(std::string s) {
    auto issp = [](unsigned char c){ return std::isspace(c)!=0; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
}
static inline std::string up_copy(std::string s) {
    for (auto &c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

// Default path beside the current DBF: <basename>.cnx
static fs::path cnx_path_from_area(const xbase::DbArea& area) {
    fs::path dbf = area.filename();           // uses existing API
    if (dbf.empty()) return {};
    dbf = fs::absolute(dbf);
    fs::path p = dbf;
    p.replace_extension(".cnx");
    return p;
}

// Parse optional path (first token). If none, use default beside DBF.
// If a bare name is given without extension, append .cnx and place beside DBF.
static bool resolve_target_path(const xbase::DbArea& area,
                                std::istringstream& args,
                                fs::path& out,
                                std::string* rest_out = nullptr)
{
    std::string rest; std::getline(args, rest);
    if (rest_out) *rest_out = rest;
    rest = trim_copy(rest);

    if (rest.empty()) {
        fs::path p = cnx_path_from_area(area);
        if (p.empty()) return false;
        out = p;
        return true;
    }

    // split first token (= path), leave remainder (if any) in rest_out
    std::string first = rest;
    auto sp = first.find_first_of(" \t");
    if (sp != std::string::npos) {
        if (rest_out) *rest_out = trim_copy(first.substr(sp+1));
        first = trim_copy(first.substr(0, sp));
    } else {
        if (rest_out) *rest_out = std::string();
    }

    fs::path p = first;
    if (p.extension() != ".cnx") p.replace_extension(".cnx");
    if (!p.has_root_path()) {
        // put beside DBF
        fs::path base = cnx_path_from_area(area);
        if (base.empty()) return false;
        p = base.parent_path() / p.filename();
    }
    out = fs::absolute(p);
    return true;
}

static void print_help() {
    std::cout
      << "CNX CREATE [<path.cnx>]\n"
      << "CNX ADDTAG <name> [<path.cnx>]\n"
      << "CNX DROPTAG <name> [<path.cnx>]\n"
      << "CNX TAGS [<path.cnx>]\n"
      << "CNX INFO [<path.cnx>]\n";
}

static inline bool file_exists(const fs::path& p) {
    std::error_code ec; return fs::exists(p, ec);
}

// ---- command ----
void cmd_CNX(xbase::DbArea& area, std::istringstream& args)
{
    std::string sub; args >> sub;
    std::string SUB = up_copy(sub);

    if (SUB.empty() || SUB == "HELP") {
        print_help();
        return;
    }

    // ---------------- CREATE ----------------
    if (SUB == "CREATE") {
        fs::path target;
        if (!resolve_target_path(area, args, target)) {
            std::cout << "CNX CREATE: unable to resolve path.\n";
            return;
        }
        if (file_exists(target)) {
            std::cout << "CNX CREATE: file already exists: \"" << target.string() << "\"\n";
            return;
        }

        CNXHandle* h = nullptr;
        if (!cnxfile::open(target.string(), h)) {
            std::cout << "CNX CREATE: open/create failed.\n";
            return;
        }
        cnxfile::CNXHeader hdr{};
        (void)cnxfile::read_header(h, hdr);
        cnxfile::close(h);
        std::cout << "CNX created: \"" << target.string() << "\"\n";
        return;
    }

    // ---------------- INFO ----------------
    if (SUB == "INFO") {
        fs::path target;
        (void)resolve_target_path(area, args, target);
        if (!file_exists(target)) {
            std::cout << "CNX: file not found: \"" << target.string() << "\"\n";
            return;
        }
        CNXHandle* h = nullptr;
        if (!cnxfile::open(target.string(), h)) {
            std::cout << "CNX INFO: open failed.\n";
            return;
        }
        cnxfile::CNXHeader hdr{};
        if (!cnxfile::read_header(h, hdr)) {
            std::cout << "CNX INFO: invalid header.\n";
            cnxfile::close(h);
            return;
        }
        std::cout << "CNX: \"" << target.string() << "\"\n"
                  << "  magic    : 0x" << std::hex << hdr.magic << std::dec << "\n"
                  << "  version  : " << hdr.version << "\n"
                  << "  page_size: " << hdr.page_size << "\n"
                  << "  flags    : 0x" << std::hex << hdr.flags << std::dec << "\n"
                  << "  tag_count: " << hdr.tag_count << "\n";
        cnxfile::close(h);
        return;
    }

    // ---------------- TAGS ----------------
    if (SUB == "TAGS") {
        fs::path target;
        (void)resolve_target_path(area, args, target);
        if (!file_exists(target)) {
            std::cout << "CNX: file not found: \"" << target.string() << "\"\n";
            return;
        }
        CNXHandle* h = nullptr;
        if (!cnxfile::open(target.string(), h)) {
            std::cout << "CNX TAGS: open failed.\n";
            return;
        }
        std::vector<cnxfile::TagInfo> tags;
        if (!cnxfile::read_tagdir(h, tags)) {
            std::cout << "CNX TAGS: read failed.\n";
            cnxfile::close(h);
            return;
        }
        if (tags.empty()) {
            std::cout << "(no tags)\n";
        } else {
            for (const auto& t : tags) {
                std::cout << "  [" << t.tag_id << "] " << t.name;
                if (t.root_page_off) std::cout << "  (built)";
                std::cout << "\n";
            }
        }
        cnxfile::close(h);
        return;
    }

    // ---------------- ADDTAG <name> ----------------
    if (SUB == "ADDTAG") {
        std::string name; args >> name;
        if (name.empty()) { std::cout << "CNX ADDTAG: missing <name>.\n"; return; }

        std::string rest; std::getline(args, rest);
        rest = trim_copy(rest);
        fs::path target;
        if (rest.empty()) {
            if (!resolve_target_path(area, args, target)) {
                std::cout << "CNX ADDTAG: unable to resolve path.\n"; return;
            }
        } else {
            std::istringstream restin(rest);
            if (!resolve_target_path(area, restin, target)) {
                std::cout << "CNX ADDTAG: unable to resolve path.\n"; return;
            }
        }
        if (!file_exists(target)) {
            std::cout << "CNX: file not found: \"" << target.string() << "\"\n";
            return;
        }

        CNXHandle* h = nullptr;
        if (!cnxfile::open(target.string(), h)) { std::cout << "CNX ADDTAG: open failed.\n"; return; }
        const std::string up = up_copy(name);
        if (!cnxfile::add_tag(h, up)) {
            std::cout << "CNX ADDTAG: tag already exists.\n";
            cnxfile::close(h); return;
        }
        cnxfile::close(h);
        std::cout << "CNX ADDTAG: added '" << up << "'.\n";
        return;
    }

    // ---------------- DROPTAG <name> ----------------
    if (SUB == "DROPTAG") {
        std::string name; args >> name;
        if (name.empty()) { std::cout << "CNX DROPTAG: missing <name>.\n"; return; }

        std::string rest; std::getline(args, rest);
        rest = trim_copy(rest);
        fs::path target;
        if (rest.empty()) {
            if (!resolve_target_path(area, args, target)) {
                std::cout << "CNX DROPTAG: unable to resolve path.\n"; return;
            }
        } else {
            std::istringstream restin(rest);
            if (!resolve_target_path(area, restin, target)) {
                std::cout << "CNX DROPTAG: unable to resolve path.\n"; return;
            }
        }
        if (!file_exists(target)) {
            std::cout << "CNX: file not found: \"" << target.string() << "\"\n";
            return;
        }

        CNXHandle* h = nullptr;
        if (!cnxfile::open(target.string(), h)) { std::cout << "CNX DROPTAG: open failed.\n"; return; }
        const std::string up = up_copy(name);
        if (!cnxfile::drop_tag(h, up)) {
            std::cout << "CNX DROPTAG: not found.\n";
            cnxfile::close(h); return;
        }
        cnxfile::close(h);
        std::cout << "CNX DROPTAG: removed '" << up << "'.\n";
        return;
    }

    std::cout << "CNX: unknown subcommand '" << SUB << "'.\n";
}
