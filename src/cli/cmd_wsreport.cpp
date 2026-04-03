// ================================
// FILE: src/cli/cmd_wsreport.cpp
// ================================

// WSREPORT (workspace + index summary + environment/settings/output routing + path state)

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "workareas.hpp"
#include "workspace/workarea_utils.hpp"
#include "index_summary.hpp"
#include "cli/order_report.hpp"
#include "cli/settings.hpp"
#include "cli/output_router.hpp"
#include "cli/path_resolver.hpp"

// Path state (SETPATH) — provides dottalk::paths::dump()
#include "cli/cmd_setpath.hpp"

using dottalk::IndexSummary;

namespace {

static std::string upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

static bool has_token(const std::string& hay, const char* tok) {
    return hay.find(tok) != std::string::npos;
}

static std::string basename_of(const std::string& path) {
    try { return std::filesystem::path(path).filename().string(); }
    catch (...) { return path; }
}

static const char* env_cstr(const char* key) {
    if (!key || !*key) return nullptr;
    return std::getenv(key);
}

static void print_kv(std::ostream& os, const char* k, const std::string& v, int w = 18) {
    os << "  " << std::left << std::setw(w) << k << ": " << v << "\n";
}
static void print_kv(std::ostream& os, const char* k, const char* v, int w = 18) {
    os << "  " << std::left << std::setw(w) << k << ": " << (v ? v : "(not set)") << "\n";
}
static void print_kv(std::ostream& os, const char* k, bool v, int w = 18) {
    os << "  " << std::left << std::setw(w) << k << ": " << (v ? "ON" : "OFF") << "\n";
}
static void print_kv(std::ostream& os, const char* k, int v, int w = 18) {
    os << "  " << std::left << std::setw(w) << k << ": " << v << "\n";
}

static std::string nz(const std::string& s, const char* empty_text = "(none)") {
    return s.empty() ? std::string(empty_text) : s;
}

static std::string safe_area_filename(const workareas::WorkArea& wa) {
    try { return wa.file_name(); } catch (...) { return {}; }
}

static std::string safe_area_label(const workareas::WorkArea& wa) {
    try { return wa.label(); } catch (...) { return {}; }
}

static bool same_slot(const workareas::WorkArea& a, const workareas::WorkArea& b) noexcept {
    return a.slot() == b.slot();
}

static std::string capacity_desc() {
    std::ostringstream out;
    const std::size_t n = workareas::count();
    if (n == 0) {
        out << "{}";
    } else {
        out << "{0.." << (n - 1) << "}";
    }
    return out.str();
}

static void print_workspace_block(std::ostream& os) {
    const auto areas = workareas::all();
    const auto cur   = areas.current();

    os << "Workspace\n";
    os << "----------------------------------------\n";
    os << "  Occupied: " << workareas::occupied_desc() << "\n";
    os << "  Open     : " << workareas::open_count() << "\n";
    os << "  Capacity : " << capacity_desc() << "\n";
    os << "  Current  : "
       << ((cur.valid() && cur.is_open()) ? std::to_string(cur.slot()) : "(none)")
       << "\n\n";

    os << "  Slot  Cur  Name                         File\n";
    os << "  ----- ---- ---------------------------- ------------------------------\n";

    bool any = false;

    for (std::size_t i = 0; i < areas.count(); ++i) {
        const auto wa = areas[i];
        if (!wa.is_open()) continue;

        any = true;

        const std::string label = nz(safe_area_label(wa), "(unnamed)");
        const std::string file  = nz(basename_of(safe_area_filename(wa)));

        os << "  "
           << std::right << std::setw(5) << wa.slot() << " "
           << std::setw(4) << (same_slot(wa, cur) ? "*" : "")
           << " "
           << std::left << std::setw(28) << label
           << " "
           << file
           << "\n";
    }

    if (!any) {
        os << "  (no open work areas)\n";
    }

    os << "\n";
}

static void print_lmdb_block(std::ostream& os) {
    os << "LMDB (per-area)\n";
    os << "----------------------------------------\n";

    bool any = false;

    const auto areas = workareas::all();
    const auto cur   = areas.current();

    for (std::size_t i = 0; i < areas.count(); ++i) {
        const auto wa = areas[i];
        if (!wa.is_open()) continue;

        xbase::DbArea* A = wa.db();
        if (!A) continue;

        const auto* im = A->indexManagerPtr();
        if (!im || !im->hasBackend() || !im->isCdx()) continue;

        any = true;

        os << "Area " << wa.slot();
        if (same_slot(wa, cur)) os << " [current]";
        os << ": " << nz(safe_area_label(wa), "(unnamed)") << "\n";

        const std::string envdir = im->containerPath().empty()
            ? std::string()
            : dottalk::paths::resolve_lmdb_env_for_cdx(im->containerPath()).string();

        print_kv(os, "STATE",  "OPEN");
        print_kv(os, "FILE",   nz(safe_area_filename(wa)));
        print_kv(os, "ENVDIR", nz(envdir, "(unknown)"));
        print_kv(os, "TAG",    nz(im->activeTag()));
        os << "\n";
    }

    if (!any) {
        print_kv(os, "STATE", "NONE");
        os << "\n";
    }
}

static void print_env_block(std::ostream& os, bool verbose) {
    os << "Environment\n";
    os << "----------------------------------------\n";

    print_kv(os, "CWD", std::filesystem::current_path().string());
#ifdef _WIN32
    print_kv(os, "OS", "Windows");
#else
    print_kv(os, "OS", "Unix");
#endif
    print_kv(os, "USER", env_cstr("USERNAME") ? env_cstr("USERNAME") : env_cstr("USER"));
    print_kv(os, "HOME", env_cstr("USERPROFILE") ? env_cstr("USERPROFILE") : env_cstr("HOME"));
    print_kv(os, "TEMP", env_cstr("TEMP") ? env_cstr("TEMP") : env_cstr("TMPDIR"));

    os << "\nDotTalk / Build variables\n";
    os << "----------------------------------------\n";

    const char* keys[] = {
        "DOTTALK_DATA",
        "DOTTALK_DBF",
        "DOTTALK_INDEXES",
        "DOTTALK_SCHEMAS",
        "DOTTALK_SCRIPTS",
        "DOTTALK_TESTS",
        "DOTTALK_HELP",
        "DOTTALK_LOGS",
        "DOTTALK_TMP",
        "TVISION_ROOT",
        "VCPKG_ROOT",
        "VCPKG_DEFAULT_TRIPLET",
        "CMAKE_GENERATOR",
        "CMAKE_TOOLCHAIN_FILE"
    };
    for (const char* k : keys) {
        print_kv(os, k, env_cstr(k));
    }

    if (verbose) {
        os << "\nPATH (verbose)\n";
        os << "----------------------------------------\n";
        print_kv(os, "PATH", env_cstr("PATH"));
    }

    os << "\n";
}

static void print_paths_block(std::ostream& os) {
    os << "Paths (SETPATH)\n";
    os << "----------------------------------------\n";

    try {
        const std::string dump = dottalk::paths::dump();
        if (dump.empty()) {
            os << "(unavailable)\n\n";
            return;
        }
        os << dump;
        if (dump.back() != '\n') os << "\n";
        os << "\n";
    } catch (...) {
        os << "(unavailable)\n\n";
    }
}

static void print_settings_block(std::ostream& os) {
    const auto& S = cli::Settings::instance();

    os << "SET / Runtime Settings\n";
    os << "----------------------------------------\n";

    print_kv(os, "SET TALK",    S.talk_on.load());
    print_kv(os, "STATUS",      S.status_on.load());
    print_kv(os, "TIME",        S.time_on.load());
    print_kv(os, "CLOCK",       S.clock_on.load());
    print_kv(os, "CONSOLE",     S.console_on.load());
    print_kv(os, "BELL",        S.bell_on.load());

    os << "\n";

    print_kv(os, "SAFETY",      S.safety_on.load());
    print_kv(os, "DELETED",     S.deleted_on.load());
    print_kv(os, "EXACT",       S.exact_on.load());
    print_kv(os, "ESCAPE",      S.escape_on.load());
    print_kv(os, "CARRY",       S.carry_on.load());
    print_kv(os, "CONFIRM",     S.confirm_on.load());
    print_kv(os, "EXCLUSIVE",   S.exclusive_on.load());
    print_kv(os, "MULTILOCKS",  S.multilocks_on.load());

    os << "\n";

    print_kv(os, "CENTURY",     S.century_on.load());
    print_kv(os, "DATE",        S.date_format);
    print_kv(os, "DECIMALS",    static_cast<int>(S.decimals));
    print_kv(os, "FIXED",       S.fixed_on.load());
    print_kv(os, "MEMOWIDTH",   static_cast<int>(S.memo_width));
    print_kv(os, "MEMOERROR",   S.memo_error_on.load());

    os << "\n";

    print_kv(os, "DEFAULT",     nz(S.default_dir));
    print_kv(os, "PATH",        nz(S.path_list));

    os << "\n";
}

static void print_output_router_block(std::ostream& os) {
    auto& R = cli::OutputRouter::instance();

    os << "Output Routing\n";
    os << "----------------------------------------\n";
    print_kv(os, "CONSOLE",        R.console_on());
    print_kv(os, "PRINT",          R.print_on());
    print_kv(os, "ALTERNATE",      R.alternate_on());
    print_kv(os, "ROUTER TALK",    R.talk_on());
    print_kv(os, "ECHO",           R.echo_on());

    print_kv(os, "PRINT TO",       nz(R.print_to_path()));
    print_kv(os, "ALTERNATE TO",   nz(R.alternate_to_path()));

    os << "\n";
}

static void print_area_index_block(std::ostream& os,
                                   const workareas::WorkArea& wa,
                                   bool isCurrent,
                                   bool verbose) {
    xbase::DbArea* A = wa.db();
    if (!A) return;

    const std::string file  = safe_area_filename(wa);
    const std::string base  = basename_of(file);
    const std::string label = nz(safe_area_label(wa), "(unnamed)");

    os << "Area " << wa.slot();
    if (isCurrent) os << " [current]";
    os << ": " << label << "\n";

    print_kv(os, "FILE", nz(file));
    print_kv(os, "BASENAME", nz(base));
    os << "\n";

    os << "Index / Order\n";
    os << "----------------------------------------\n";

    orderreport::print_status_block(os, *A);

    const IndexSummary S = dottalk::summarize_index(*A);

    if (S.tags.empty()) {
        print_kv(os, "Tags", "(none)");
    } else if (!verbose) {
        std::ostringstream tag_line;
        bool first = true;
        for (const auto& t : S.tags) {
            if (!first) tag_line << ", ";
            tag_line << (t.tagName.empty() ? t.fieldName : t.tagName);
            first = false;
        }
        print_kv(os, "Tags", tag_line.str());
    } else {
        os << "  Tags              :\n\n";
        os << "  " << std::left << std::setw(14) << "Field Name"
           << std::setw(10) << "Type"
           << std::setw(6)  << "Len"
           << std::setw(6)  << "Dec"
           << "Dir\n";
        os << "  ------------ ----- ------ ------ ----\n";
        for (const auto& T : S.tags) {
            const std::string field_name = !T.fieldName.empty() ? T.fieldName : T.tagName;
            os << "  " << std::left << std::setw(14) << field_name
               << std::setw(10) << (T.type.empty() ? "" : T.type)
               << std::setw(6)  << T.len
               << std::setw(6)  << T.dec
               << (T.asc ? "ASC" : "DESC") << "\n";
        }
    }

    print_kv(os, "Records",    static_cast<int>(A->recCount()));
    print_kv(os, "Recno",      static_cast<int>(A->recno()));
    print_kv(os, "Record len", static_cast<int>(A->recLength()));
    print_kv(os, "Fields",     static_cast<int>(A->fieldCount()));
    os << "\n";
}

} // namespace

// WSREPORT [ALL] [VERBOSE] [ENV] [PATHS] [SET] [ROUTE]
void cmd_WSREPORT(xbase::DbArea& /*A*/, std::istringstream& args) {
    auto& out = cli::OutputRouter::instance().out();

    const std::string raw = upper_copy(args.str());

    const bool wantAll     = has_token(raw, "ALL");
    const bool wantVerbose = has_token(raw, "VERBOSE") || has_token(raw, "INDEX");

    const bool wantEnvOnly   = has_token(raw, "ENV");
    const bool wantPathsOnly = has_token(raw, "PATHS");
    const bool wantSetOnly   = has_token(raw, "SET");
    const bool wantRouteOnly = has_token(raw, "ROUTE");

    const bool anyExplicit = wantEnvOnly || wantPathsOnly || wantSetOnly || wantRouteOnly;

    const bool showEnv   = anyExplicit ? wantEnvOnly   : true;
    const bool showPaths = anyExplicit ? wantPathsOnly : true;
    const bool showSet   = anyExplicit ? wantSetOnly   : true;
    const bool showRoute = anyExplicit ? wantRouteOnly : true;

    const auto areas = workareas::all();
    const auto cur   = areas.current();

    out << "DotTalk Status Report\n\n";

    print_workspace_block(out);
    print_lmdb_block(out);

    if (showEnv)   print_env_block(out, wantVerbose);
    if (showPaths) print_paths_block(out);
    if (showSet)   print_settings_block(out);
    if (showRoute) print_output_router_block(out);

    out << "Areas / Index Summary\n";
    out << "----------------------------------------\n";

    if (wantAll) {
        bool anyOpen = false;
        for (std::size_t i = 0; i < areas.count(); ++i) {
            const auto wa = areas[i];
            if (!wa.is_open()) continue;
            anyOpen = true;
            print_area_index_block(out, wa, same_slot(wa, cur), wantVerbose);
        }
        if (!anyOpen) {
            out << "(no open work areas)\n\n";
        }
    } else {
        if (cur.valid() && cur.is_open()) {
            print_area_index_block(out, cur, true, wantVerbose);
        } else {
            out << "Area (none)\n";
            out << "\nIndex / Order\n";
            out << "----------------------------------------\n";
            print_kv(out, "Order", "PHYSICAL");
            print_kv(out, "Index file", "(none)");
            print_kv(out, "Active tag", "(none)");
            print_kv(out, "Sort", "(none)");
            print_kv(out, "Tags", "(none)");
            print_kv(out, "Records", 0);
            print_kv(out, "Recno", 0);
            print_kv(out, "Record len", 0);
            print_kv(out, "Fields", 0);
            out << "\n";
        }
    }

    out.flush();
}
