#include <iostream>
#include <sstream>
#include <string>
#include "textio.hpp"
#include "predicates.hpp"
#include "xbase.hpp"
#include "../../include/command_registry.hpp"
// optional if you use bare 'reg' instead of cli::reg:
using cli::reg;


using namespace std;

// Minimal parser for: LOCATE FOR <field> <op> <value>
// Example:  LOCATE FOR LASTNAME = SMITH
//           LOCATE FOR AGE > 40
// Value parsing is token-based (no quoted strings yet); extend as needed.
static bool parse_for_clause(std::istringstream& iss,
                             std::string& fld, std::string& op, std::string& val)
{
    std::string kw;
    if (!(iss >> kw)) return false;
    if (textio::up(kw) != "FOR") return false;

    if (!(iss >> fld)) return false;
    if (!(iss >> op )) return false;
    if (!(iss >> val)) return false;

    return true;
}

namespace {

void cmd_LOCATE(xbase::DbArea& area, std::istringstream& iss)
{
    if (!area.isOpen()) {
        std::cout << "No table is open. Use USE <file> first.\n";
        return;
    }

    std::string fld, op, val;
    if (!parse_for_clause(iss, fld, op, val)) {
        std::cout << "Syntax: LOCATE FOR <field> <op> <value>\n";
        return;
    }

    // Start at current record; if not valid, TOP()
    int32_t start = area.recno();
    if (start <= 0) area.top();

    // Read current before evaluation to sync buffer
    if (!area.readCurrent()) {
        // Try at TOP as a fallback
        area.top();
        area.readCurrent();
    }

    // Scan from current to EOF
    bool found = false;
    do {
        if (predicates::eval(area, fld, op, val)) {
            std::cout << "Found at recno " << area.recno() << "\n";
            found = true;
            break;
        }
    } while (area.skip(+1) && area.readCurrent());

    if (!found) {
        std::cout << "Not found.\n";
    }
}

} // namespace

// Register command
static bool s_registered = [](){
    static cli::CommandRegistry reg;
    reg.add("LOCATE", &cmd_LOCATE);
    return true;
}();
