#include <sstream>
#include <iostream>
#include <string>
#include "xbase.hpp"

// existing handler we want to reuse
void cmd_SETINDEX(xbase::DbArea&, std::istringstream&);

static std::string next_token_upper(std::istringstream& iss) {
    std::string t;
    if (!(iss >> t)) return {};
    for (auto& c : t) c = (char)std::toupper((unsigned char)c);
    return t;
}

void cmd_SET(xbase::DbArea& a, std::istringstream& args) {
    // SET <what> [args...]
    auto what = next_token_upper(args);
    if (what == "INDEX") {
        // Accept both: "SET INDEX ln" and "SET INDEX TO ln"
        auto maybeTo = next_token_upper(args);
        if (maybeTo != "TO" && !maybeTo.empty()) {
            // we actually grabbed the tag already; push it back into a new stream
            std::istringstream reparsed(maybeTo + " " + std::string(std::istreambuf_iterator<char>(args),
                                                                    std::istreambuf_iterator<char>()));
            cmd_SETINDEX(a, reparsed);
            return;
        }
        // read the tag after optional TO
        std::string tag;
        args >> tag;
        std::istringstream reparsed(tag);
        cmd_SETINDEX(a, reparsed);
        return;
    }

    std::cout << "Unknown SET option: " << what << "\n";
}
