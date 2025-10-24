// src/cli/cmd_cnx.cpp — CNX utility command (full drop-in)
//
// Subcommands (current area assumed if no path given):
//
//   CNX CREATE [<path.cnx>]
//   CNX ADDTAG <name> FIELDS <F1[+F2[+...]]> [ASC|DESC] [<path.cnx>]
//   CNX DROPTAG <name> [<path.cnx>]
//   CNX REBUILD [TAGS <t1[,t2[,...]]>] [<path.cnx>]
//   CNX INFO [<path.cnx>]
//   CNX TAGS [<path.cnx>]
//   CNX COMPACT [<path.cnx>]
//
// Notes:
// - All file paths default to a sibling CNX next to the current area's DBF.
// - This file does NOT change active ordering; INX/CNX selection is elsewhere.

#include <sstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <cctype>
#include <memory>

#include "xbase.hpp"                 // xbase::DbArea
#include "order_state.hpp"           // close other indexes in area when working with CNX
#include "cnx/cnx.hpp"               // CNX API

#include "xindex/index_manager.hpp"
#include "xindex/index_spec.hpp"
#include "xindex/index_key.hpp"
#include "xindex/attach.hpp"         // ensure_manager(...)
#include "xindex/dbarea_adapt.hpp"   // db_record_count, db_is_deleted, db_get_string/double

namespace fs = std::filesystem;

// ---------- small helpers ----------
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

// Put CNX beside DBF, return absolute path.
static fs::path cnx_path_from_area(const xbase::DbArea& area) {
    fs::path dbf = area.filename();
    if (dbf.empty()) return {};
    dbf = fs::absolute(dbf);
    fs::path p = dbf;
    p.replace_extension(".cnx");
    return p; // absolute, sibling of DBF
}

// Resolve optional arg; bare names go beside DBF, add .cnx if missing.
// Always return absolute path.
static bool resolve_target_path(const xbase::DbArea& area,
                                std::istringstream& args,
                                fs::path& out,
                                std::string* rest_out = nullptr)
{
    auto trim = [](std::string s){
        auto issp = [](unsigned char c){ return std::isspace(c)!=0; };
        while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
        return s;
    };

    std::string rest; std::getline(args, rest);
    if (rest_out) *rest_out = rest;
    rest = trim(rest);

    if (rest.empty()) {
        fs::path p = cnx_path_from_area(area);
        if (p.empty()) return false;
        out = fs::absolute(p);
        return true;
    }

    fs::path arg(rest);
    if (arg.has_parent_path() || arg.extension() == ".cnx" || arg.extension() == ".CNX") {
        if (arg.extension().empty()) arg.replace_extension(".cnx");
        out = fs::absolute(arg);
        return true;
    }

    // Bare filename -> beside DBF
    fs::path dbf = area.filename();
    if (!dbf.empty()) {
        fs::path p = fs::absolute(dbf).parent_path() / arg;
        if (p.extension().empty()) p.replace_extension(".cnx");
        out = fs::absolute(p);
        return true;
    }

    // Fallback: CWD, but still absolute
    if (arg.extension().empty()) arg.replace_extension(".cnx");
    out = fs::absolute(arg);
    return true;
}

static void print_usage() {
    std::cout
      << "CNX command:\n"
      << "  CNX CREATE [<path.cnx>]\n"
      << "  CNX ADDTAG <name> FIELDS <F1[+F2[+...]]> [ASC|DESC] [<path.cnx>]\n"
      << "  CNX DROPTAG <name> [<path.cnx>]\n"
      << "  CNX REBUILD [TAGS <t1[,t2[,...]]>] [<path.cnx>]\n"
      << "  CNX INFO [<path.cnx>]\n"
      << "  CNX TAGS [<path.cnx>]\n"
      << "  CNX COMPACT [<path.cnx>]\n";
}

// ---- key encoding (match IndexManager’s sortable scheme) ----
static inline std::string up(const std::string& s) {
    std::string r = s;
    for (auto &c : r) c = (char)std::toupper((unsigned char)c);
    return r;
}
static inline void encode_double_sortable(double d, std::vector<uint8_t>& out) {
    union { double d; uint64_t u; } u{d};
    u.u ^= (1ull<<63);                 // flip sign bit
    uint8_t be[8];
    for (int i=0;i<8;++i) be[7-i] = (uint8_t)((u.u>>(i*8)) & 0xFF);
    out.insert(out.end(), be, be+8);
}
static std::vector<uint8_t> encode_key_bytes(const xindex::IndexKey& k) {
    std::vector<uint8_t> out; out.reserve(32);
    for (size_t i=0;i<k.parts.size(); ++i) {
        if (i) out.push_back(0x1F); // unit separator
        const auto& a = k.parts[i];
        if (std::holds_alternative<std::string>(a)) {
            std::string s = up(std::get<std::string>(a));
            out.insert(out.end(), (const uint8_t*)s.data(), (const uint8_t*)s.data()+s.size());
            out.push_back(0x00);
        } else {
            encode_double_sortable(std::get<double>(a), out);
        }
    }
    return out;
}
// --------------------------------------------------------------

// Ensure no other index is active (INX/CDX/etc.) before mutating CNX.
static void close_conflicting_indexes(xbase::DbArea& area) {
    try { orderstate::clearOrder(area); } catch (...) {}
    try { xindex::ensure_manager(area).clear_active(); } catch (...) {}
}

// Build (or rebuild) a specific set of tags into CNX.
static bool rebuild_tags_into_cnx(xbase::DbArea& area, const fs::path& cnxPath,
                                  const std::vector<std::string>& tagsOrEmptyAll)
{
    std::unique_ptr<cnx::CNXFile> f;
    try {
        if (!cnxPath.parent_path().empty() && !fs::exists(cnxPath.parent_path()))
            fs::create_directories(cnxPath.parent_path());
        if (fs::exists(cnxPath)) f = std::make_unique<cnx::CNXFile>(cnxPath.string());
        else {
            auto created = cnx::CNXFile::CreateNew(cnxPath.string());
            (void)created;
            f = std::make_unique<cnx::CNXFile>(cnxPath.string());
        }
    } catch (const std::exception& e) {
        std::cout << "CNX: open/create failed for '" << cnxPath.string() << "': " << e.what() << "\n";
        return false;
    }

    auto& mgr = xindex::ensure_manager(area);
    std::vector<std::string> tags;
    if (tagsOrEmptyAll.empty()) {
        tags = mgr.listTags(); // all in-memory tags
        if (tags.empty()) {
            std::cout << "CNX REBUILD: no tags in memory. Use CNX ADDTAG first (or attach an index).\n";
            return false;
        }
    } else {
        tags = tagsOrEmptyAll;
    }

    for (const auto& tag : tags) {
        // Derive spec from expr (F1+F2+...)
        xindex::IndexSpec spec; spec.tag = tag;
        if (auto expr = mgr.exprFor(tag); !expr.empty()) {
            size_t i=0;
            while (i < expr.size()) {
                size_t j = expr.find('+', i);
                std::string part = expr.substr(i, (j==std::string::npos? expr.size() : j) - i);
                // trim
                part.erase(part.begin(), std::find_if(part.begin(), part.end(), [](unsigned char c){return !std::isspace(c);}));
                while (!part.empty() && std::isspace((unsigned char)part.back())) part.pop_back();
                if (!part.empty()) spec.fields.push_back(part);
                if (j == std::string::npos) break;
                i = j + 1;
            }
        }

        try {
            f->rebuildTag(tag, [&](auto sink){
                const int n = xindex::db_record_count(area);
                for (int rec=1; rec<=n; ++rec) {
                    if (xindex::db_is_deleted(area, rec)) continue;
                    xindex::IndexKey key = mgr.key_from_record(spec, rec);
                    cnx::KeyEntry e;
                    e.key   = encode_key_bytes(key);
                    e.recno = (uint32_t)rec;
                    sink(std::move(e));
                }
            });
        } catch (const std::exception& e) {
            std::cout << "CNX REBUILD: tag '" << tag << "' failed: " << e.what() << "\n";
            return false;
        }
    }

    try {
        f->compactAll();
    } catch (const std::exception& e) {
        std::cout << "CNX: compact failed: " << e.what() << "\n";
        return false;
    }
    return true;
}

static std::vector<std::string> split_csv_list(const std::string& s) {
    std::vector<std::string> out;
    size_t i=0;
    while (i<s.size()) {
        size_t j = s.find(',', i);
        std::string t = s.substr(i, (j==std::string::npos ? s.size() : j) - i);
        t = trim_copy(t);
        if (!t.empty()) out.push_back(t);
        if (j == std::string::npos) break;
        i = j + 1;
    }
    return out;
}

void cmd_CNX(xbase::DbArea& area, std::istringstream& in)
{
    std::string cmd;
    if (!(in >> cmd)) { print_usage(); return; }
    cmd = up_copy(cmd);

    // We’ll parse the rest later on demand (so CREATE, INFO, etc. can reuse resolver)
    std::string raw_rest;
    fs::path target;

    // ------------------ CREATE ------------------
    if (cmd == "CREATE") {
        close_conflicting_indexes(area); // avoid file locks / conflicts
        if (!resolve_target_path(area, in, target, &raw_rest)) {
            std::cout << "CNX CREATE: cannot resolve target path.\n"; return;
        }
        try {
            if (!target.parent_path().empty() && !fs::exists(target.parent_path()))
                fs::create_directories(target.parent_path());

            if (fs::exists(target)) {
                std::cout << "CNX CREATE: already exists: " << target.string() << "\n";
            } else {
                auto created = cnx::CNXFile::CreateNew(target.string());
                (void)created; // destructor may finalize header
                // Verify by opening immediately
                cnx::CNXFile verify(target.string());
                (void)verify;
                std::cout << "CNX CREATE: created " << target.string() << "\n";
            }
        } catch (const std::exception& e) {
            std::cout << "CNX CREATE: failed for " << target.string() << ": " << e.what() << "\n";
        }
        return;
    }

    // ------------------ ADDTAG ------------------
    if (cmd == "ADDTAG") {
        close_conflicting_indexes(area); // ensure no legacy INX/CDX is active
        // We need: name, 'FIELDS', expression, optional ASC|DESC, optional path
        std::string line = raw_rest;
        if (line.empty()) {
            std::getline(in, line);
        }
        line = trim_copy(line);
        if (line.empty()) {
            std::cout << "CNX ADDTAG: missing arguments.\n"; print_usage(); return;
        }

        std::istringstream ls(line);
        std::string tagName; ls >> tagName;
        std::string kw; ls >> kw;
        if (tagName.empty() || up_copy(kw) != "FIELDS") {
            std::cout << "CNX ADDTAG: expected: ADDTAG <name> FIELDS <F1[+F2[+...]]> [ASC|DESC] [<path.cnx>]\n";
            return;
        }
        std::string fieldsExpr; ls >> fieldsExpr;
        if (fieldsExpr.empty()) {
            std::cout << "CNX ADDTAG: missing fields expression.\n"; return;
        }
        std::string dirOrPath; std::getline(ls, dirOrPath); dirOrPath = trim_copy(dirOrPath);

        bool ascending = true;
        std::string pathTail;
        if (!dirOrPath.empty()) {
            auto upkw = up_copy(dirOrPath);
            if (upkw.rfind("ASC",0)==0) {
                ascending = true;
            } else if (upkw.rfind("DESC",0)==0) {
                ascending = false;
                // strip leading DESC token
                auto pos = dirOrPath.find_first_of(" \t");
                pathTail = (pos==std::string::npos) ? "" : trim_copy(dirOrPath.substr(pos+1));
            } else {
                pathTail = dirOrPath;
            }
        }

        // Ensure manager and create/ensure the tag in memory
        auto& mgr = xindex::ensure_manager(area);
        xindex::IndexSpec spec;
        spec.tag      = tagName;
        spec.ascending= ascending;

        // split F1+F2+...
        {
            size_t i=0;
            while (i < fieldsExpr.size()) {
                size_t j = fieldsExpr.find('+', i);
                std::string part = fieldsExpr.substr(i, (j==std::string::npos? fieldsExpr.size() : j) - i);
                // trim
                part.erase(part.begin(), std::find_if(part.begin(), part.end(), [](unsigned char c){return !std::isspace(c);}));
                while (!part.empty() && std::isspace((unsigned char)part.back())) part.pop_back();
                if (!part.empty()) spec.fields.push_back(part);
                if (j == std::string::npos) break;
                i = j + 1;
            }
        }
        (void)mgr.ensure_tag(spec); // builds in memory from current table

        // Resolve CNX path (use tail if any, else default next to DBF)
        fs::path cnxTarget;
        {
            std::istringstream tails(pathTail);
            if (!resolve_target_path(area, tails, cnxTarget)) {
                cnxTarget = cnx_path_from_area(area);
            }
        }

        // Rebuild just this tag into CNX (and compact)
        if (rebuild_tags_into_cnx(area, cnxTarget, std::vector<std::string>{tagName})) {
            std::cout << "CNX ADDTAG: tag '" << tagName << "' built into " << cnxTarget.string() << "\n";
        }
        return;
    }

    // ------------------ DROPTAG ------------------
    if (cmd == "DROPTAG") {
        close_conflicting_indexes(area);
        if (!resolve_target_path(area, in, target, &raw_rest)) {
            std::cout << "CNX DROPTAG: cannot resolve target path.\n"; return;
        }
        std::string line = trim_copy(raw_rest);
        if (line.empty()) { std::cout << "CNX DROPTAG: missing <name>.\n"; return; }
        // name may be alone or followed by a path in the same line; split first token
        std::string tagName, remainder;
        {
            std::istringstream ls(line);
            ls >> tagName;
            std::getline(ls, remainder);
            remainder = trim_copy(remainder);
        }
        // If remainder looks like a path, re-resolve:
        if (!remainder.empty()) {
            std::istringstream rs(remainder);
            (void)resolve_target_path(area, rs, target);
        }
        try {
            cnx::CNXFile f(target.string());
            f.dropTag(tagName);
            f.compactAll();
            std::cout << "CNX DROPTAG: '" << tagName << "' removed from " << target.string() << "\n";
        } catch (const std::exception& e) {
            std::cout << "CNX DROPTAG: failed: " << e.what() << "\n";
        }
        return;
    }

    // For the remaining subcommands, resolve the target now.
    if (!resolve_target_path(area, in, target)) {
        std::cout << "CNX: cannot resolve target path.\n";
        return;
    }

    // ------------------ INFO ------------------
    if (cmd == "INFO") {
        try {
            cnx::CNXFile f(target.string());
            auto names = f.listTagNames();
            size_t entries = 0;
            for (const auto& n : names) {
                if (const auto* t = f.getTag(n)) entries += t->base.size();
            }
            std::cout << "CNX INFO: " << target.string() << "\n"
                      << "  Tags:     " << names.size() << "\n"
                      << "  Entries:  " << entries << "\n";
        } catch (const std::exception& e) {
            std::cout << "CNX: open failed: " << e.what() << "\n";
        }
        return;
    }

    // ------------------ TAGS ------------------
    if (cmd == "TAGS") {
        try {
            cnx::CNXFile f(target.string());
            auto names = f.listTagNames();
            std::cout << "CNX TAGS: " << target.string() << "\n";
            if (names.empty()) { std::cout << "  (none)\n"; return; }
            for (const auto& n : names) {
                const auto* t = f.getTag(n);
                const size_t total = t ? t->base.size() : 0;
                std::cout << "  - " << n
                          << "  (base=" << total
                          << ", ins=0, del=0, total="<< total << ")\n";
            }
        } catch (const std::exception& e) {
            std::cout << "CNX: open failed: " << e.what() << "\n";
        }
        return;
    }

    // ------------------ COMPACT ------------------
    if (cmd == "COMPACT") {
        close_conflicting_indexes(area);
        try { cnx::CNXFile f(target.string()); f.compactAll();
              std::cout << "CNX: compacted " << target.string() << "\n";
        } catch (const std::exception& e) {
            std::cout << "CNX: open/compact failed: " << e.what() << "\n";
        }
        return;
    }

    // ------------------ REBUILD ------------------
    if (cmd == "REBUILD") {
        close_conflicting_indexes(area);
        // Support: CNX REBUILD [TAGS t1[,t2,...]] [<path>]
        // We already consumed the path; parse TAGS from raw_rest.
        std::vector<std::string> tags;
        if (!raw_rest.empty()) {
            auto uprest = up_copy(raw_rest);
            auto p = uprest.find("TAGS");
            if (p != std::string::npos) {
                std::string after = trim_copy(raw_rest.substr(p + 4));
                size_t sp = after.find_first_of(" \t");
                std::string list = (sp==std::string::npos) ? after : after.substr(0, sp);
                tags = split_csv_list(list);
            }
        }
        if (rebuild_tags_into_cnx(area, target, tags)) {
            std::cout << "CNX: rebuilt and compacted " << target.string() << "\n";
        }
        return;
    }

    std::cout << "CNX: unknown subcommand '" << cmd << "'.\n";
    print_usage();
}
