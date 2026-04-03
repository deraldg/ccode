#include <sstream>
#include <iostream>
#include <string>
#include <algorithm>

#include "xbase.hpp"

using namespace std;
using xbase::DbArea;

// CMDREL: Print the exact recipe to create indexes and wire the relation.
// (No engine/registry calls; just a friendly scripted guide.)
void cmd_CMDREL(DbArea& /*A*/, std::istringstream& /*S*/) {
    cout << "CMDREL: How to relate COMMANDS -> CMD_ARGS by COMMAND\n";
    cout << "\n";
    cout << "1) CHILD (cmd_args):\n";
    cout << "   SELECT cmd_args\n";
    cout << "   INDEX ON COMMAND TAG CMDA\n";
    cout << "   SETINDEX CMDA.inx\n";
    cout << "   SETORDER CMDA\n";
    cout << "\n";
    cout << "2) PARENT (commands):\n";
    cout << "   SELECT commands\n";
    cout << "   INDEX ON COMMAND TAG CMDP\n";
    cout << "   SETINDEX CMDP.inx\n";
    cout << "   SETORDER CMDP\n";
    cout << "\n";
    cout << "3) Wire relation (same raw field on both sides):\n";
    cout << "   SET RELATIONS TO COMMAND INTO cmd_args\n";
    cout << "\n";
    cout << "4) Sanity:\n";
    cout << "   SELECT commands\n";
    cout << "   SEEK \"APPEND\"     && requires the COMMAND index to be active\n";
    cout << "   SELECT cmd_args\n";
    cout << "   LIST FOR COMMAND=\"APPEND\"\n";
    cout << "\n";
    cout << "Notes:\n";
    cout << " - If you prefer expression-based keys (e.g. UPPER(RTRIM(COMMAND))),\n";
    cout << "   use the same expression for both parent and child index tags *and* in SET RELATIONS.\n";
}



