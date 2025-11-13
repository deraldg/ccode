// src/cli/cmd_set.cpp — FoxPro-like SET command scaffold
#include "xbase.hpp"
#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>

#include "cli/settings.hpp"

// Existing handler we want to reuse
void cmd_SETINDEX(xbase::DbArea&, std::istringstream&);

// Forward to relations/unique subsystems if present
void cmd_SET_RELATIONS(xbase::DbArea&, std::istringstream&);
void cmd_SET_UNIQUE(xbase::DbArea&, std::istringstream&);

static std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}
static std::string rest(std::istringstream& iss) {
    return std::string(std::istreambuf_iterator<char>(iss), std::istreambuf_iterator<char>());
}
static bool parse_on_off(const std::string& tok, bool& out) {
    const auto u = to_upper(tok);
    if (u == "ON")  { out = true;  return true; }
    if (u == "OFF") { out = false; return true; }
    return false;
}

void cmd_SET(xbase::DbArea& A, std::istringstream& args) {
    using cli::Settings;
    auto& S = Settings::instance();

    std::string what; if (!(args >> what)) { std::cout << "Usage: SET <option> [args]\n"; return; }
    what = to_upper(what);

    if (what == "INDEX") {
        // Accept both: SET INDEX <arg> and SET INDEX TO <arg>
        std::string maybe; args >> maybe;
        if (to_upper(maybe) != "TO" && !maybe.empty()) {
            std::istringstream reparsed(maybe + " " + rest(args));
            cmd_SETINDEX(A, reparsed);
            return;
        }
        std::string tag; args >> tag;
        std::istringstream reparsed(tag);
        cmd_SETINDEX(A, reparsed);
        return;
    }

    if (what == "RELATION" || what == "RELATIONS") {
        std::istringstream r(rest(args)); cmd_SET_RELATIONS(A, r); return;
    }
    if (what == "UNIQUE") {
        std::istringstream r(rest(args)); cmd_SET_UNIQUE(A, r); return;
    }

    if (what == "DELETED") {
        std::string tok; args >> tok; bool on = S.deleted_on.load();
        if (!parse_on_off(tok, on)) { std::cout << "SET DELETED ON|OFF\n"; return; }
        S.deleted_on.store(on);
        std::cout << "Deleted visibility: " << (on ? "HIDE (ON)" : "SHOW (OFF)") << "\n";
        return;
    }

    if (what == "TALK") {
        std::string tok; args >> tok; bool on = S.talk_on.load();
        if (!parse_on_off(tok, on)) { std::cout << "SET TALK ON|OFF\n"; return; }
        S.talk_on.store(on);
        std::cout << "Talk is " << (on ? "ON" : "OFF") << "\n";
        return;
    }

    // Fallback
    std::cout << "Unknown or unimplemented SET option: " << what << "\n";
}
