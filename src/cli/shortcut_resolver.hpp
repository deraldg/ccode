// src/cli/shortcut_resolver.hpp
#pragma once

#include <string>
#include <unordered_map>
#include <sstream>      // std::istringstream
#include "textio.hpp"   // textio::up()

namespace cli {

class ShortcutResolver {
public:
    // Expands a shortcut at the start of the input line, preserving the rest
    // of the line exactly (spacing, casing, etc.) after the first token.
    [[nodiscard]] static std::string resolve(const std::string& input_line);

private:
    static const std::unordered_map<std::string, std::string>& get_map();
};

// ---------------------------------------------------------------------------
// Inline implementation (header-only)
// ---------------------------------------------------------------------------
inline const std::unordered_map<std::string, std::string>& ShortcutResolver::get_map()
{
    static const std::unordered_map<std::string, std::string> map = {
        // ---- Help ----------------------------------------------------------
        { "H",            "HELP" },
        { "FH",           "FOXHELP" },
        { "CH",           "CMDHELP" },
        { "COMMANDSHELP", "CMDHELP" },
        { "CHK",          "CMDHELPCHK" },
        { "PS",           "PSHELL" },

        // ---- UI launchers --------------------------------------------------
        { "BT",           "BROWSETUI" },
        { "BTV",          "TVISION" },
        { "LU",           "LMDB_UTIL"},
        { "REPEAT",       "ECHO" },
        { "TURBOTALK",    "FOXTALK"},

        // ---- Misc / legacy aliases ----------------------------------------
        { "!",            "BANG" },
        { "?",            "FORMULA" },
        { "UNDELETE",     "RECALL" },
        { "EXACT",        "SETCASE"},

        // ---- Relations -----------------------------------------------------
        { "SET_RELATION", "SET RELATION" },
        { "REL",          "REL" },
        { "REL_LIST",     "REL_LIST" },
        { "RELTALK",      "REL" },

        // ---- Tuple / relational tooling -----------------------------------
        { "TUP",          "TUPLE" },
        { "TUPTALK",      "TUPTALK" },
        { "TT",           "TUPTALK" },
        { "TABLES",       "TABLE" },

        // ---- Scripts -------------------------------------------------------
        { "DO",           "DOTSCRIPT" },
        { "RUN",          "DOTSCRIPT" },

        // ---- Smartlist / loops --------------------------------------------
        { "SL",           "SMARTLIST" },
        { "LP",           "LOOP" },
        { "EL",           "ENDLOOP" },
        { "LL",           "LIST_LMDB" },

        // ---- SQL-ish shortcuts (your originals) ----------------------------
        { "WH",           "SQL" },

        { "SM",           "SMARTBROWSER" },
        { "SMART",        "SMARTBROWSER" },

        // ---- Simple Browser -----------------------------------------------
        { "SB",           "SIMPLEBROWSER" },
        { "WS",           "SIMPLEBROWSER" },
        { "WORKSPACE",    "SIMPLEBROWSER" },

        // ---- “fast typist” / concatenation accidents ----------------------
        { "SMARTLISTFOR", "SMARTLIST" },
        { "SLFOR",        "SMARTLIST" },
        { "SEL",          "SELECT" },
        { "BOOL",         "BOOLEAN"},
        { "S",            "SELECT"},
        { "DESC",         "DESCEND"}

    };

    return map;
}

inline std::string ShortcutResolver::resolve(const std::string& input_line)
{
    if (input_line.empty()) return input_line;

    std::istringstream iss(input_line);
    std::string first_token;
    iss >> first_token;
    if (first_token.empty()) return input_line;

    const auto& map = get_map();
    auto it = map.find(textio::up(first_token));
    if (it == map.end()) return input_line;  // no shortcut => original

    const std::string& full_cmd = it->second;

    // Replace only the first token, preserve everything else as-is.
    // Note: find(first_token) will locate the first occurrence, which is the
    // one we just parsed (because >> skips leading whitespace).
    const size_t pos = input_line.find(first_token);
    if (pos == std::string::npos) {
        // Fallback: just prepend the expansion
        return full_cmd + " " + input_line;
    }

    const size_t end = pos + first_token.size();

    // If the line is exactly the shortcut token, return just the expanded cmd.
    if (end >= input_line.size()) return full_cmd;

    return input_line.substr(0, pos) + full_cmd + input_line.substr(end);
}

} // namespace cli
