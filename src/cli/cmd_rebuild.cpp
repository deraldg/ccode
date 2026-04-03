// src/cli/cmd_rebuild.cpp
// REBUILD [<name-or-path.cnx>]
//
// CNX rebuild (legacy compound index)
//
// Policy:
// - Enumerates CNX tags
// - Rebuilds each tag using field-based expressions (v1)
// - Requires clean TABLE (or explicit COMMIT)
//
// Architecture:
// - Public CNX container resolves via INDEXES
// - CNX remains a legacy/flat structure (no LMDB backend)
//
// Relationship to REINDEX:
// - REINDEX CNX -> calls REBUILD
// - This file is the CNX execution engine

#include "xbase.hpp"

#include "cnx/cnx.hpp"
#include "cli/path_resolver.hpp"
#include "cli/order_state.hpp"
#include "xindex/cnx_backend.hpp"
#include "cli/table_state.hpp"

#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

extern "C" xbase::XBaseEngine* shell_engine(void);

// forward declare
void cmd_COMMIT(xbase::DbArea& A, std::istringstream& in);

namespace {

static inline std::string up_copy(std::string s)
{
    for (auto& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static int resolve_current_index(xbase::DbArea& A) {
    if (auto* eng = shell_engine()) {
        for (int i = 0; i < xbase::MAX_AREA; ++i) {
            if (&eng->area(i) == &A) return i;
        }
    }
    return -1;
}

static bool prompt_yes_no(const std::string& prompt, bool default_no = true)
{
    std::cout << prompt;
    std::cout << (default_no ? " (y/N) " : " (Y/n) ");
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return !default_no;
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(line[0])));
    return c == 'Y';
}

static std::string normalize_field_name(std::string s)
{
    auto nul = s.find('\0');
    if (nul != std::string::npos) s.resize(nul);

    while (!s.empty() && s.back() == ' ') s.pop_back();

    for (auto& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static fs::path resolve_cnx_token(const std::string& tok)
{
    fs::path p = dottalk::paths::resolve_index(tok);
    if (!p.has_extension()) p.replace_extension(".cnx");
    return p;
}

static fs::path default_cnx_for_open_table(const xbase::DbArea& A)
{
    if (orderstate::hasOrder(A) && orderstate::isCnx(A)) {
        const std::string active = orderstate::orderName(A);
        if (!active.empty()) return fs::path(active);
    }

    std::string stem = A.dbfBasename();
    if (stem.empty()) stem = A.logicalName();
    if (stem.empty()) stem = "table";

    return resolve_cnx_token(stem);
}

static bool field_exists_for_tag(const xbase::DbArea& A, const std::string& tag_name_anycase)
{
    const std::string want = normalize_field_name(tag_name_anycase);

    const auto& fdefs = A.fields();
    for (const auto& f : fdefs) {
        const std::string have = normalize_field_name(f.name);
        if (have == want) return true;
    }
    return false;
}

static bool ensure_clean_or_commit(xbase::DbArea& A, int area0, const char* verb)
{
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

static void print_help()
{
    std::cout
        << "REBUILD [<name-or-path.cnx>]\n"
        << "  Rebuild all CNX tags (field-name tags only).\n"
        << "  CNX must exist.\n"
        << "  No args: uses current CNX or defaults to <table>.cnx\n";
}

} // namespace

void cmd_REBUILD(xbase::DbArea& A, std::istringstream& in)
{
    const int area0 = resolve_current_index(A);
    if (!ensure_clean_or_commit(A, area0, "REBUILD")) return;

    std::string arg;
    if (in >> arg) {
        const std::string up = up_copy(arg);
        if (up == "HELP" || up == "/?" || up == "-H" || up == "--HELP") {
            print_help();
            return;
        }
    }

    if (!A.isOpen()) {
        std::cout << "REBUILD: no table open.\n";
        return;
    }

    fs::path cnx_path;
    if (!arg.empty()) cnx_path = resolve_cnx_token(arg);
    else              cnx_path = default_cnx_for_open_table(A);

    if (!fs::exists(cnx_path)) {
        std::cout << "REBUILD: CNX not found: " << cnx_path.string() << "\n";
        return;
    }

    std::cout << "REINDEX CNX -> REBUILD\n";
    std::cout << "CNX container: " << cnx_path.string() << "\n";

    cnxfile::CNXHandle* h = nullptr;
    if (!cnxfile::open(cnx_path.string(), h)) {
        std::cout << "REBUILD: unable to open CNX\n";
        return;
    }

    std::vector<cnxfile::TagInfo> tags;
    if (!cnxfile::read_tagdir(h, tags)) {
        std::cout << "REBUILD: failed to read tag directory\n";
        cnxfile::close(h);
        return;
    }

    if (tags.empty()) {
        std::cout << "REBUILD: (no tags)\n";
        cnxfile::close(h);
        return;
    }

    int ok = 0, skip = 0, fail = 0;

    for (const auto& t : tags) {
        const std::string tag = normalize_field_name(t.name);

        if (!field_exists_for_tag(A, tag)) {
            std::cout << "  [" << t.tag_id << "] " << tag << " : SKIP\n";
            ++skip;
            continue;
        }

        try {
            xindex::CnxBackend b(A, cnx_path.string(), tag);
            b.open(cnx_path.string());
            b.rebuild();
            b.close();

            std::cout << "  [" << t.tag_id << "] " << tag << " : OK\n";
            ++ok;
        }
        catch (...) {
            std::cout << "  [" << t.tag_id << "] " << tag << " : FAIL\n";
            ++fail;
        }
    }

    cnxfile::close(h);

    if (fail == 0 && dottalk::table::is_enabled(area0)) {
        dottalk::table::set_stale(area0, false);
        dottalk::table::clear_stale_fields(area0);
        std::cout << "REBUILD: TABLE STALE cleared (fresh)\n";
    }

    std::cout << "REBUILD: done  OK=" << ok
              << "  SKIP=" << skip
              << "  FAIL=" << fail << "\n";
}