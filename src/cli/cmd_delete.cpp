#include <iostream>
#include <sstream>
#include <string>
#include "textio.hpp"
#include "predicates.hpp"
#include "xbase.hpp"
#include "command_registry.hpp"

// optional if you use bare 'reg' instead of cli::registry():
// using cli::registry();

using namespace std;

// Helper: parse "FOR <field> <op> <value>"
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

// IMPORTANT: external linkage (no anonymous namespace), because shell references cmd_DELETE directly.
void cmd_DELETE(xbase::DbArea& area, std::istringstream& iss)
{
    if (!area.isOpen()) {
        std::cout << "No table is open. Use USE <file> first.\n";
        return;
    }

    // Default behavior: DELETE (no args) => delete CURRENT record only
    std::string tok;
    std::streampos savepos = iss.tellg();
    if (!(iss >> tok)) {
        // No args => current
        if (!area.readCurrent()) { std::cout << "No current record.\n"; return; }
        if (area.deleteCurrent()) std::cout << "1 deleted\n";
        else std::cout << "0 deleted\n";
        return;
    }
    iss.seekg(savepos); // rewind to reparse per mode

    // Modes: ALL | REST | NEXT n | FOR fld op val
    std::string fld, op, val;
    if (parse_for_clause(iss, fld, op, val)) {
        // FOR mode: scan all records and delete matches
        int32_t deleted = 0;
        if (!area.top()) { std::cout << "0 deleted\n"; return; }
        if (!area.readCurrent()) { std::cout << "0 deleted\n"; return; }
        do {
            if (predicates::eval(area, fld, op, val)) {
                if (area.deleteCurrent()) ++deleted;
            }
        } while (area.skip(+1) && area.readCurrent());
        std::cout << deleted << " deleted\n";
        return;
    }

    std::string mode;
    iss >> mode;
    std::string umode = textio::up(mode);

    if (umode == "ALL") {
        int32_t deleted = 0;
        if (area.top() && area.readCurrent()) {
            do {
                if (area.deleteCurrent()) ++deleted;
            } while (area.skip(+1) && area.readCurrent());
        }
        std::cout << deleted << " deleted\n";
        return;
    }

    if (umode == "REST") {
        int32_t deleted = 0;
        // Ensure current is valid; if not, try TOP
        if (!area.readCurrent()) {
            if (!area.top() || !area.readCurrent()) { std::cout << "0 deleted\n"; return; }
        }
        do {
            if (area.deleteCurrent()) ++deleted;
        } while (area.skip(+1) && area.readCurrent());
        std::cout << deleted << " deleted\n";
        return;
    }

    if (umode == "NEXT") {
        int n = 0;
        if (!(iss >> n) || n <= 0) { std::cout << "Usage: DELETE NEXT <n>\n"; return; }
        int32_t deleted = 0;
        // Ensure current is valid; if not, try TOP
        if (!area.readCurrent()) {
            if (!area.top() || !area.readCurrent()) { std::cout << "0 deleted\n"; return; }
        }
        for (int i = 0; i < n; ++i) {
            // Delete current if possible
            if (area.deleteCurrent()) ++deleted;
            // Move to next; stop if EOF
            if (!(area.skip(+1) && area.readCurrent())) break;
        }
        std::cout << deleted << " deleted\n";
        return;
    }

    // If none matched, treat first token as unexpected and print usage
    std::cout << "Usage: DELETE [ALL | REST | NEXT <n> | FOR <field> <op> <value>]"
              << "  (no args => delete current record)\n";
}

// Self-register with the command registry (kept for CLI support path)
static bool s_registered = [](){
    static cli::CommandRegistry reg;
    cli::registry().add("DELETE", &cmd_DELETE);
    return true;
}();
