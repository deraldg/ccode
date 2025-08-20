// src/cli/cmd_help.cpp
#include "xbase.hpp"
#include <iostream>
#include <sstream>

void cmd_HELP(xbase::DbArea&, std::istringstream&) {
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
        << "QUIT | EXIT\n";
}
