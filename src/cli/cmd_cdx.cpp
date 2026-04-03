// src/cli/cmd_cdx.cpp
// CDX utility command (CREATE/INFO/TAGS/ADDTAG/DROPTAG)
//
// Behavior change requested:
//   - If invoked with no arguments (". cdx"), print help/options and return.
//     (Do NOT default to INFO.)
//
// Policy enforced:
//   - CREATE: refuse if file exists.
//   - INFO/TAGS/ADDTAG/DROPTAG: require existing file (no implicit creation).
//   - Default path (no path arg):
//       1) if area has active CDX in orderstate: use it
//       2) else <current_dbf_basename>.cdx resolved via INDEXES slot
//
// Notes:
//   - Tag build data (RUN1) persistence is handled by cdx backend; this command
//     is only for container header + tag directory management.

#include "xbase.hpp"
#include "cdx/cdx.hpp"
#include "cli/path_resolver.hpp"
#include "cli/order_state.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using cdxfile::CDXHandle;

namespace {

static inline std::string trim_copy(std::string s)
{
    auto issp = [](unsigned char c){ return std::isspace(c)!=0; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}

static inline std::string up_copy(std::string s)
{
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static inline bool file_exists(const fs::path& p)
{
    std::error_code ec;
    return fs::exists(p, ec);
}

static fs::path resolve_cdx_token(const std::string& tok)
{
    fs::path p = dottalk::paths::resolve_index(tok);
    if (!p.has_extension()) p.replace_extension(".cdx");
    return p;
}

static fs::path default_cdx_path(xbase::DbArea& area)
{
    // 1) If a CDX is currently active in orderstate, reuse its path directly.
    if (orderstate::hasOrder(area) && orderstate::isCnx(area)) {
        const std::string active = orderstate::orderName(area);
        if (!active.empty()) return fs::path(active);
    }

    // 2) Otherwise, if a table is open, use dbfBasename/logicalName/name-stem.
    std::string stem;
    if (area.isOpen()) {
        stem = area.dbfBasename();
        if (stem.empty()) stem = area.logicalName();
        if (stem.empty()) {
            fs::path n(area.name());
            stem = n.stem().string();
        }
        if (stem.empty()) stem = "table";
    } else {
        stem = "table";
    }

    fs::path p = resolve_cdx_token(stem);
    if (!p.has_extension()) p.replace_extension(".cdx");
    return p;
}

// Parse optional path (first token from remainder). If none, use default policy.
static bool resolve_target_path(xbase::DbArea& area,
                                std::istringstream& args,
                                fs::path& out)
{
    std::string rest;
    std::getline(args, rest);
    rest = trim_copy(rest);

    if (rest.empty()) {
        out = default_cdx_path(area);
        return !out.empty();
    }

    // first token = path-ish
    std::string first = rest;
    auto sp = first.find_first_of(" \t");
    if (sp != std::string::npos) first = trim_copy(first.substr(0, sp));

    fs::path p = resolve_cdx_token(first);
    if (!p.has_extension()) p.replace_extension(".cdx");
    out = p;
    return true;
}

static void print_help()
{
    std::cout
        << "CDX INFO [<path.cdx>]\n"
        << "CDX TAGS [<path.cdx>]\n"
        << "CDX CREATE [<path.cdx>]\n"
        << "CDX ADDTAG <name> [<path.cdx>]\n"
        << "CDX DROPTAG <name> [<path.cdx>]\n";
}

} // namespace

// ---- command ----
void cmd_CDX(xbase::DbArea& area, std::istringstream& args)
{
    std::string sub;
    args >> sub;

    // NEW: ". cdx" (no args) prints options/help and returns.
    if (sub.empty()) {
        print_help();
        return;
    }

    std::string SUB = up_copy(sub);

    if (SUB == "HELP" || SUB == "/?" || SUB == "-H" || SUB == "--HELP") {
        print_help();
        return;
    }

    // ---------------- CREATE ----------------
    if (SUB == "CREATE") {
        fs::path target;
        if (!resolve_target_path(area, args, target)) {
            std::cout << "CDX CREATE: unable to resolve path.\n";
            return;
        }
        if (file_exists(target)) {
            std::cout << "CDX CREATE: file already exists: \"" << target.string() << "\"\n";
            return;
        }

        CDXHandle* h = nullptr;
        if (!cdxfile::open(target.string(), h)) {
            std::cout << "CDX CREATE: open/create failed.\n";
            return;
        }
        cdxfile::CDXHeader hdr{};
        (void)cdxfile::read_header(h, hdr);
        cdxfile::close(h);

        std::cout << "CDX created: \"" << target.string() << "\"\n";
        return;
    }

    // ---------------- INFO / TAGS ----------------
    if (SUB == "INFO" || SUB == "TAGS") {
        fs::path target;
        (void)resolve_target_path(area, args, target);

        if (!file_exists(target)) {
            std::cout << "CDX: file not found: \"" << target.string() << "\"\n";
            return;
        }

        CDXHandle* h = nullptr;
        if (!cdxfile::open(target.string(), h)) {
            std::cout << "CDX: unable to open: \"" << target.string() << "\"\n";
            return;
        }

        if (SUB == "INFO") {
            cdxfile::CDXHeader hdr{};
            if (!cdxfile::read_header(h, hdr)) {
                std::cout << "CDX INFO: invalid header.\n";
                cdxfile::close(h);
                return;
            }

            std::vector<cdxfile::TagInfo> tags;
            (void)cdxfile::read_tagdir(h, tags);

            std::cout << "CDX file : " << target.string() << "\n";
            std::cout << "Tags     : " << tags.size() << "\n";
            for (const auto& t : tags) {
                std::cout << "  [" << t.tag_id << "] " << t.name
                          << "  root_off=" << t.root_page_off
                          << "  recs=" << t.stats_rec
                          << "\n";
            }
            cdxfile::close(h);
            return;
        }

        // TAGS
        std::vector<cdxfile::TagInfo> tags;
        if (!cdxfile::read_tagdir(h, tags)) {
            std::cout << "CDX TAGS: read failed.\n";
            cdxfile::close(h);
            return;
        }
        if (tags.empty()) {
            std::cout << "(no tags)\n";
        } else {
            for (const auto& t : tags) {
                std::cout << "  [" << t.tag_id << "] " << t.name << "\n";
            }
        }
        cdxfile::close(h);
        return;
    }

    // ---------------- ADDTAG <name> ----------------
    if (SUB == "ADDTAG") {
        std::string name;
        args >> name;
        if (name.empty()) {
            std::cout << "CDX ADDTAG: missing <name>.\n";
            return;
        }

        fs::path target;
        if (!resolve_target_path(area, args, target)) {
            std::cout << "CDX ADDTAG: unable to resolve path.\n";
            return;
        }
        if (!file_exists(target)) {
            std::cout << "CDX: file not found: \"" << target.string() << "\"\n";
            return;
        }

        CDXHandle* h = nullptr;
        if (!cdxfile::open(target.string(), h)) {
            std::cout << "CDX ADDTAG: open failed.\n";
            return;
        }

        const std::string up = up_copy(name);
        if (!cdxfile::add_tag(h, up)) {
            std::cout << "CDX ADDTAG: tag already exists.\n";
            cdxfile::close(h);
            return;
        }

        cdxfile::close(h);
        std::cout << "CDX ADDTAG: added '" << up << "'.\n";
        return;
    }

    // ---------------- DROPTAG <name> ----------------
    if (SUB == "DROPTAG") {
        std::string name;
        args >> name;
        if (name.empty()) {
            std::cout << "CDX DROPTAG: missing <name>.\n";
            return;
        }

        fs::path target;
        if (!resolve_target_path(area, args, target)) {
            std::cout << "CDX DROPTAG: unable to resolve path.\n";
            return;
        }
        if (!file_exists(target)) {
            std::cout << "CDX: file not found: \"" << target.string() << "\"\n";
            return;
        }

        CDXHandle* h = nullptr;
        if (!cdxfile::open(target.string(), h)) {
            std::cout << "CDX DROPTAG: open failed.\n";
            return;
        }

        const std::string up = up_copy(name);
        if (!cdxfile::drop_tag(h, up)) {
            std::cout << "CDX DROPTAG: not found.\n";
            cdxfile::close(h);
            return;
        }

        cdxfile::close(h);
        std::cout << "CDX DROPTAG: removed '" << up << "'.\n";
        return;
    }

    std::cout << "CDX: unknown subcommand: " << sub << "\n";
}