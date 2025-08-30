// ===============================
// File: src/cli/cmd_help.cpp
// Removes ZAP from the help list, clarifies SETORDER usage.
// ===============================
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "textio.hpp"

void cmd_HELP(xbase::DbArea& /*a*/, std::istringstream& /*args*/) {
    using std::cout; using std::endl;

    cout << "CREATE <name> (<field specs>)     # make a new DBF (C/N/D/L)\n";
    cout << "APPEND [BLANK|-B] [n]              # append n blank records (default 1)\n";
    cout << "REPLACE <field> WITH <value>       # edit current record (C/N/D/L)\n";
    cout << "EDIT <field> WITH <value>          # alias of REPLACE\n";
    cout << "DUMP [TOP n] [fields...]           # vertical record dump\n";
    cout << "DISPLAY                            # show current record (field=value)\n";
    cout << "STATUS | STRUCT | RECNO | TOP | BOTTOM | GOTO <n>\n";
    cout << "CLEAR | CLS                        # clear the screen\n";
    cout << "COLOR <GREEN|AMBER|DEFAULT>\n";
    cout << "DIR                                 # list DBF files\n";
    cout << "USE <dbf> [NOINDEX]                 # open a DBF\n";
    cout << "LIST [n|ALL] | FIELDS | COUNT\n";
    cout << "INDEX ON <field> TAG <name> [ASC|DESC]\n";
    cout << "SEEK <field> <value>             # exact match in current order/index; '=' optional\n";
    cout << "FIND <field> <needle>            # case-insensitive substring search (e.g., FIND FIRST_NAME \"Diana\")\n";
    cout << "ASCEND | DESCEND | SETINDEX <tag> | SETORDER <n|tag>\n";
    cout << "REFRESH                              # refresh current view/index\n";
    cout << "COPY | EXPORT <table> TO <file> | IMPORT <table> FROM <file> [FIELDS (...)] [LIMIT n]\n";
    cout << "DELETE | RECALL | PACK\n";
    cout << "VERSION | BANG\n";
    cout << "QUIT | EXIT\n\n";

    cout << "Commands:\n";
    cout << "  HELP              USE               QUIT\n";
    cout << "  EXIT              LIST              FIELDS\n";
    cout << "  COUNT             TOP               BOTTOM\n";
    cout << "  GOTO              RECNO             APPEND\n";
    cout << "  DELETE            DISPLAY           RECALL\n";
    cout << "  PACK              COPY              EXPORT\n";
    cout << "  IMPORT            COLOR             REPLACE\n";
    cout << "  EDIT              CREATE            STATUS\n";
    cout << "  STRUCT            INDEX             SEEK\n";
    cout << "  FIND              LOCATE            ASCEND\n";
    cout << "  DESCEND           SETINDEX          SETORDER\n";
    cout << "  DIR               REFRESH           VERSION\n";
    cout << "  TEST              FOXHELP           BANG\n";
}
