#include "command_registry.hpp"
#include "textio.hpp"
#include <vector>
#include <unordered_set>
#include <iomanip>

namespace cli {

void CommandRegistry::add(std::string name, Handler h) {
    map_.emplace(textio::up(name), std::move(h));
}

bool CommandRegistry::run(const std::string& name, xbase::DbArea& area, std::istringstream& iss) const {
    auto it = map_.find(textio::up(name));
    if (it == map_.end()) return false;
    it->second(area, iss);
    return true;
}

void CommandRegistry::help(std::ostream& os) const {
static const std::vector<std::string> verbs = {
    // built-ins (handled in shell.cpp)
    "HELP","AREA","SELECT","USE","QUIT","EXIT",

    // implemented commands (registered in shell.cpp)
    "LIST","COUNT","TOP","BOTTOM","GOTO",
    "APPEND","DELETE","UNDELETE","DISPLAY","RECALL","PACK",
    "COPY","EXPORT","IMPORT","COLOR",

    // planned / not-yet-implemented (will show with * in help())
    "REPLACE","CREATE","STATUS","STRUCT","INDEX","SEEK","FIND","LOCATE","SET","BROWSE","SKIP"
};
    const std::unordered_set<std::string> builtins = {"AREA","SELECT","QUIT","EXIT"};

    os << "Commands (* = not available yet):\n";
    constexpr int COLS = 3;
    constexpr int COLW = 18;
    int col = 0;
    for (const auto& v : verbs) {
        const std::string V = textio::up(v);
        const bool implemented = builtins.count(V) || (map_.find(V) != map_.end());
        std::string cell = v + (implemented ? "" : " *");
        if (col == 0) os << "  ";
        os << std::left << std::setw(COLW) << cell;
        col = (col + 1) % COLS;
        if (col == 0) os << "\n";
    }
    if (col != 0) os << "\n";
}

} // namespace cli
