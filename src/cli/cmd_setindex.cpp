// TEMP STUB to restore build after accidental edits.
// Replace with real implementation later.

#include <sstream>
#include <string>
#include <iostream>

namespace xbase { class DbArea; }
class ShellContext;

// Variant A handler: void cmd_SETINDEX(xbase::DbArea&, std::istringstream&)
void cmd_SETINDEX(xbase::DbArea& /*d*/, std::istringstream& iss) {
    std::string tag;
    if (!(iss >> tag)) {
        std::cout << "Usage: SETINDEX <tag>\n";
        return;
    }
    std::cout << "Order set to TAG " << tag << " (stub)\n";
}

// Variant B handler: void cmd_SETINDEX(std::istringstream&, ShellContext&)
void cmd_SETINDEX(std::istringstream& iss, ShellContext& /*ctx*/) {
    std::string tag;
    if (!(iss >> tag)) {
        std::cout << "Usage: SETINDEX <tag>\n";
        return;
    }
    std::cout << "Order set to TAG " << tag << " (stub)\n";
}
