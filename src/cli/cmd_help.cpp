// src/cli/cmd_help.cpp
#include "cmd_help.hpp"
#include "cmd_help_router.hpp"
#include "help_beta.hpp"
#include "foxref.hpp"
#include "dotref.hpp"
#include "edref.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <system_error>

#include "xbase.hpp"

// Optional setpath-aware slot resolving.
#if __has_include("cli/path_resolver.hpp") && __has_include("cli/cmd_setpath.hpp")
  #include "cli/path_resolver.hpp"
  #include "cli/cmd_setpath.hpp"
  #define HAVE_PATHS 1
#else
  #define HAVE_PATHS 0
#endif

// ---------------------------------------------------------------------------
// Registered command handlers (dispatchable via dli::registry)
// ---------------------------------------------------------------------------
extern void cmd_CMDHELP(xbase::DbArea&, std::istringstream&);   // HELP GIANT / BUILD
extern void cmd_FOXHELP(xbase::DbArea&, std::istringstream&);   // FoxPro quick ref
extern void cmd_PREDHELP(xbase::DbArea&, std::istringstream&);  // Predicate help

// ---------------------------------------------------------------------------
// Specialized help renderers (called directly with string argument)
// ---------------------------------------------------------------------------
extern void show_pshell_help(const std::string&);
extern void show_sql_help(const std::string&);

namespace {

namespace fs = std::filesystem;

inline std::string uptrim(std::string s) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(std::begin(s), std::find_if(std::begin(s), std::end(s), notspace));
    s.erase(std::find_if(std::rbegin(s), std::rend(s), notspace).base(), std::end(s));
    std::transform(std::begin(s), std::end(s), std::begin(s),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static fs::path help_root_dir() {
#if HAVE_PATHS
    try {
        namespace paths = dottalk::paths;
        return paths::get_slot(paths::Slot::HELP);
    } catch (...) {
        // fall through to conventional subdir detection
    }
#endif
    std::error_code ec;
    fs::path cand = fs::current_path(ec) / "help";
    if (!ec && fs::exists(cand, ec) && fs::is_directory(cand, ec)) {
        return cand;
    }
    return fs::current_path(ec);
}

// ---------------------------------------------------------------------------
// Local catalog renderers
// ---------------------------------------------------------------------------

inline bool show_dot_topic(const std::string& term) {
    if (term.empty()) return false;
    if (const auto* it = dotref::find(term)) {
        std::cout << it->name << "\n"
                  << "  " << it->syntax << "\n"
                  << "  " << it->summary << "\n";
        return true;
    }
    return false;
}

inline bool show_ed_topic(const std::string& term) {
    if (term.empty()) return false;
    if (const auto* it = edref::find(term)) {
        std::cout << it->topic << "\n"
                  << "  " << it->syntax << "\n"
                  << it->summary << "\n";
        return true;
    }
    return false;
}

inline bool show_fox_topic_local(const std::string& term) {
    if (term.empty()) return false;
    if (const auto* it = foxref::find(term)) {
        std::cout << it->name << "\n"
                  << "  " << it->syntax << "\n"
                  << "  " << it->summary << "\n";
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Replaced run_cmd with direct calls (no run_cmd needed)
// ---------------------------------------------------------------------------
inline void show_fox(xbase::DbArea& area, const std::string& term) {
    std::istringstream iss(term);
    cmd_FOXHELP(area, iss);
}

inline void show_predicates(xbase::DbArea& area) {
    std::istringstream empty;
    cmd_PREDHELP(area, empty);
}

inline void show_beta_router(const std::string& restUp) {
    std::string term;
    if (restUp.size() > 4) {
        term = restUp.substr(4);
        while (!term.empty() && std::isspace(static_cast<unsigned char>(term.front()))) {
            term.erase(term.begin());
        }
        if (term == "CHECKLIST" || term == "STATUS") {
            term.clear();
        }
    }
    dottalk::help::show_beta(term);
}

} // anonymous namespace

void cmd_HELP(xbase::DbArea& area, std::istringstream& args)
{
    using namespace dottalk::help;

    std::string rest;
    {
        std::ostringstream oss;
        oss << args.rdbuf();
        rest = oss.str();
    }
    const std::string restUp = uptrim(rest);

    if (restUp == "GIANT" || restUp == "/GIANT") {
        std::istringstream empty;
        cmd_CMDHELP(area, empty);
        return;
    }

    if (restUp == "BETA" || restUp.rfind("BETA ", 0) == 0) {
        show_beta_router(restUp);
        return;
    }
    if (restUp.rfind("BETA-", 0) == 0) {
        dottalk::help::show_beta(restUp);
        return;
    }

    // PS / PSHELL / POWERSHELL support via HELP
    if (restUp == "PS" || restUp == "PSHELL" || restUp == "POWERSHELL" ||
        restUp.rfind("PS ", 0) == 0 ||
        restUp.rfind("PSHELL ", 0) == 0 ||
        restUp.rfind("POWERSHELL ", 0) == 0) {
        std::string ps_arg;
        size_t space = rest.find(' ');
        if (space != std::string::npos) {
            ps_arg = rest.substr(space + 1);
        }
        show_pshell_help(ps_arg);
        return;
    }

    // SQL / SQLHELP routing
    if (restUp == "SQL" || restUp == "SQLHELP" ||
        restUp.rfind("SQL ", 0) == 0 ||
        restUp.rfind("SQLHELP ", 0) == 0) {
        std::string sql_arg;
        size_t space = rest.find(' ');
        if (space != std::string::npos) {
            sql_arg = rest.substr(space + 1);
        }
        show_sql_help(sql_arg);
        return;
    }

    if (restUp.empty()) {
        // Replaced missing show_help_about_help with inline message
        std::cout << "DotTalk++ Help System\n\n"
                  << "  HELP GIANT         - full command catalog\n"
                  << "  HELP BETA          - beta checklist\n"
                  << "  HELP PS / PSHELL   - PowerShell helpers\n"
                  << "  HELP SQL           - SQL reference (SQLite + MSSQL)\n"
                  << "  HELP PREDICATES    - COUNT/LOCATE syntax\n"
                  << "  HELP /FOX <topic>  - FoxPro compatibility reference\n"
                  << "  HELP /DOT <topic>  - DotTalk-native command reference\n"
                  << "  HELP /ED <topic>   - Educational/system concepts\n"
                  << "  HELP <command>     - default topic lookup\n";
        return;
    }

    auto opts = parse_opts(rest);

    if (opts.isBuild) {
        std::istringstream build_iss("BUILD");
        cmd_CMDHELP(area, build_iss);
        return;
    }
    if (opts.predOnly) {
        show_predicates(area);
        return;
    }
    if (opts.onlyFox) {
        show_fox(area, opts.term);
        return;
    }
    if (opts.onlyDot) {
        if (!show_dot_topic(opts.term)) {
            std::cout << "No DotTalk help found for: " << opts.term << "\n";
        }
        return;
    }
    if (opts.onlyEd) {
        if (!show_ed_topic(opts.term)) {
            std::cout << "No educational help found for: " << opts.term << "\n";
        }
        return;
    }

    if (!opts.term.empty()) {
        std::string term_up = uptrim(opts.term);
        if (term_up == "BETA" || term_up.rfind("BETA ", 0) == 0) {
            show_beta_router(term_up);
            return;
        }
        if (show_dot_topic(opts.term)) return;
        if (show_ed_topic(opts.term)) return;
        if (show_fox_topic_local(opts.term)) return;
        show_fox(area, opts.term);
        return;
    }

    // Fallback when no match
    std::cout << "Type HELP GIANT, HELP BETA, HELP PS, HELP SQL, or HELP <command>\n";
}