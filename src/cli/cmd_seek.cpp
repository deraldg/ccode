// TEMP STUB to restore build after accidental edits.
// Replace with real implementation later.

#include <sstream>
#include <string>
#include <iostream>

namespace xbase { class DbArea; }
class ShellContext;

// Variant A handler: void cmd_SEEK(xbase::DbArea&, std::istringstream&)
void cmd_SEEK(xbase::DbArea& /*d*/, std::istringstream& iss) {
    std::string key;
    if (!(iss >> key)) {
        std::cout << "Usage: SEEK <key>\n";
        return;
    }
    std::cout << "Not found. (stub)\n";
}

// Variant B handler: void cmd_SEEK(std::istringstream&, ShellContext&)
void cmd_SEEK(std::istringstream& iss, ShellContext& /*ctx*/) {
    std::string key;
    if (!(iss >> key)) {
        std::cout << "Usage: SEEK <key>\n";
        return;
    }
    std::cout << "Not found. (stub)\n";
}
