// src/cli/shell_api.cpp
#include "shell_api.hpp"

#include "command_registry.hpp"
#include "xbase.hpp"
#include "textio.hpp"

#include <cctype>
#include <sstream>
#include <string>
#include <iostream>

using xbase::DbArea;

static inline bool is_comment_or_blank(const std::string& s0) {
    // trim left
    size_t i = 0;
    while (i < s0.size() && std::isspace(static_cast<unsigned char>(s0[i]))) ++i;
    if (i >= s0.size()) return true;                           // blank
    if (s0[i] == '#' || s0[i] == ';') return true;             // #... ;...
    if (s0[i] == '/' && i + 1 < s0.size() && s0[i+1] == '/')   // //...
        return true;
    return false;
}

bool shell_dispatch_line(DbArea& area, const std::string& line) {
    // Skip blanks/comments
    if (is_comment_or_blank(line)) return true;

    // Normalize/parse the verb
    std::string s = textio::trim(line);
    std::istringstream iss(s);

    std::string verb;
    iss >> verb;
    if (!iss && verb.empty()) return true;

    std::string name = textio::up(verb);

    // Combine multi-word verbs: SET INDEX [TO]
    if (name == "SET") {
        std::streampos p2 = iss.tellg();
        std::string w2; iss >> w2;
        if (iss && textio::up(w2) == "INDEX") {
            std::streampos p3 = iss.tellg();
            std::string w3; iss >> w3;
            if (iss && textio::up(w3) == "TO") {
                name = "SET INDEX TO";
            } else {
                // back up if it's not "TO"
                iss.clear();
                iss.seekg(p3);
                name = "SET INDEX";
            }
        } else {
            // back up if it's not "INDEX"
            iss.clear();
            iss.seekg(p2);
        }
    }

    // Treat EXIT as QUIT for the REPL caller
    if (name == "EXIT") name = "QUIT";
    if (name == "QUIT") return false; // signal shell exit

    // Dispatch via command registry
    const bool handled = cli::registry().run(area, name, iss);
    if (!handled) {
        std::cout << "Unknown command. Type HELP.\n";
    }
    return true; // handled (including unknown/usage cases)
}
