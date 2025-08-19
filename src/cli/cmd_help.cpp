// src/cli/cmd_help.cpp
#include "xbase.hpp"
#include <iostream>
#include <sstream>

// Signature must match the registry: void cmd_HELP(DbArea&, std::istringstream&)
void cmd_HELP(xbase::DbArea&, std::istringstream&) {
    // Quick reference (newer commands you just added)
    std::cout
        << "CREATE <name> (<field specs>)     # make a new DBF (C/N/D/L)\n"
        << "APPEND_BLANK [n]                  # append n blank records (non-interactive)\n"
        << "REPLACE <field> WITH <value>      # edit current record (C/N/D/L)\n"
        << "EDIT <field> WITH <value>         # alias of REPLACE\n"
        << "DUMP [TOP n] [fields...]          # vertical record dump\n"
        << "DISPLAY                           # show current record (field=value)\n"
        << "STATUS | STRUCT | RECNO | TOP | BOTTOM\n"
        << "CLEAR | CLS                       # clear the screen\n"
        << "COLOR <GREEN|AMBER|DEFAULT>\n"
        << "QUIT | EXIT\n\n";

    // Legacy/other commands you already show in HELP (keep for completeness)
    std::cout
        << "Commands (* = not available yet):\n"
        << "  HELP              AREA              SELECT\n"
        << "  USE               QUIT              EXIT\n"
        << "  LIST              FIELDS            COUNT\n"
        << "  TOP               BOTTOM            GOTO\n"
        << "  APPEND            DELETE            UNDELETE\n"
        << "  DISPLAY           RECALL            PACK\n"
        << "  COPY              EXPORT            IMPORT\n"
        << "  COLOR             REPLACE           CREATE\n"
        << "  STATUS            STRUCT            INDEX *\n"
        << "  SEEK              FIND              LOCATE *\n"
        << "  SET *             BROWSE *          SKIP *\n";
}
