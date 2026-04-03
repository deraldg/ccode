// src/cli/cmd_setpath_command.cpp
// DotTalk++: SETPATH command implementation (CLI)
//
// Usage:
//   SETPATH                 -> show current roots
//   SETPATH RESET           -> restore defaults (based on data root)
//   SETPATH <SLOT> <path>   -> set slot path
//
// Relative path behavior:
//   - Relative paths are rooted under current DATA.
//   - If DATA already ends with "data", then both:
//         indexes
//         data/indexes
//     normalize to the same target.
//
// Slots:
//   DATA DBF xDBF INDEXES LMDB WORKSPACES SCHEMAS PROJECTS SCRIPTS TESTS HELP LOGS TMP

#include "xbase.hpp"
#include "cli/cmd_setpath.hpp"

namespace {

static std::filesystem::path find_data_root_guess()
{
    namespace fs = std::filesystem;
    fs::path p = fs::current_path();
    for (int i = 0; i < 14; ++i) {
        fs::path cand = p / "data";
        if (fs::exists(cand) && fs::is_directory(cand)) {
            return fs::absolute(cand);
        }
        if (!p.has_parent_path()) break;
        fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return fs::absolute(fs::current_path());
}

} // namespace

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

static std::string read_word(std::istringstream& iss) {
    std::string w;
    iss >> w;
    return w;
}

static std::string read_rest(std::istringstream& iss) {
    std::string s;
    std::getline(iss >> std::ws, s);
    while (!s.empty() && (s.back()=='\r' || s.back()=='\n' || s.back()==' ' || s.back()=='\t')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i]==' ' || s[i]=='\t')) ++i;
    return s.substr(i);
}

static std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

void cmd_SETPATH(xbase::DbArea&, std::istringstream& iss)
{
    std::string a1 = read_word(iss);
    if (a1.empty()) {
        std::cout << dottalk::paths::dump();
        return;
    }

    std::string u1 = up(a1);
    if (u1 == "RESET") {
        if (dottalk::paths::state().data_root.empty()) {
            dottalk::paths::init_defaults(find_data_root_guess());
        } else {
            dottalk::paths::reset();
        }
        std::cout << "SETPATH: reset to defaults.\n";
        std::cout << dottalk::paths::dump();
        return;
    }

    dottalk::paths::Slot slot{};
    if (!dottalk::paths::slot_from_string(a1, slot)) {
        std::cout << "SETPATH: unknown slot: " << a1 << "\n";
        std::cout << "Usage: SETPATH [RESET] | SETPATH <DATA|DBF|xDBF|INDEXES|LMDB|WORKSPACES|SCHEMAS|PROJECTS|SCRIPTS|TESTS|HELP|LOGS|TMP> <path>\n";
        return;
    }

    std::string rest = read_rest(iss);
    if (rest.empty()) {
        std::cout << "Usage: SETPATH " << dottalk::paths::slot_name(slot) << " <path>\n";
        return;
    }

    dottalk::paths::set_slot(slot, fs::path(rest));
    std::cout << "SETPATH: " << dottalk::paths::slot_name(slot)
              << " = " << dottalk::paths::get_slot(slot).string() << "\n";
}
