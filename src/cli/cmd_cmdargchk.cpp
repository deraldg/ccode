// src/cli/cmd_cmdargchk.cpp
#include <iostream>
#include <sstream>
#include <string>
#include "xbase.hpp"

// NOTE: Keep this light-weight: no private DbArea calls, no engine singletons.
// This build now emits guidance for the materialized help metadata join key.
void cmd_CMDARGCHK(xbase::DbArea& /*area*/, std::istringstream& args)
{
    std::string rest;
    std::getline(args, rest); // consume line (optional)

    std::cout
        << "CMDARGCHK: quick sanity checker (stub).\n"
        << "  HELP metadata now includes CMDKEY, a stored join key built as CATALOG|COMMAND.\n"
        << "  Use CMDKEY for simple-field indexing/relations with the current engine.\n"
        << "  Note: CMDHELP BUILD writes tables under the HELP path, not the DBF root.\n"
        << "  Try the following sequence if you want to validate relations:\n"
        << "    USE help\\commands IN 1\n"
        << "    USE help\\cmd_args IN 0\n"
        << "    SELECT 1\n"
        << "    INDEX ON CMDKEY TAG CMDP\n"
        << "    SETINDEX CMDP.inx\n"
        << "    SETORDER CMDP\n"
        << "    SELECT 0\n"
        << "    INDEX ON CMDKEY TAG CMDA\n"
        << "    SETINDEX CMDA.inx\n"
        << "    SETORDER CMDA\n"
        << "    SELECT 1\n"
        << "    REL ADD commands cmd_args ON CMDKEY\n"
        << "    REL LIST\n"
        << "  Then run examples like:\n"
        << "    SELECT 1\n"
        << "    LIST 5 FOR CATALOG=\"FOX\"\n"
        << "    REL JOIN\n"
        << "  Or inspect the child table directly:\n"
        << "    SELECT 0\n"
        << "    LIST 10 FOR CMDKEY=\"FOX|APPEND\"\n";
}
