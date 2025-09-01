#include "dli/cmd_set.hpp"
#include "dli/set_view.hpp"
#include <string>
#include <cctype>

namespace dli {

static std::string up(std::string s){
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

void cmd_SET(xbase::DbArea& db, std::istringstream& args){
    std::string tok;
    if (!(args >> tok)) {
        // No args -> do nothing or print help
        return;
    }
    std::string head = up(tok);
    if (head == "VIEW") {
        // Forward remaining tail to SET VIEW
        std::string tail;
        std::getline(args, tail);
        std::istringstream iss(tail);
        cmd_SET_VIEW(db, iss);
        return;
    }
    // You can add more subcommands: COLOR, EXACT, SAFETY, etc.
    // For now, unknown -> ignore or log.
}

} // namespace dli
