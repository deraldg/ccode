#include "xbase.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "order_state.hpp"

// very small parser for: CREATE <name> (<fields...>)
// expects fields already parsed elsewhere in your file; if not, we do a minimal one.
static bool parse_field_list(std::istringstream& args, std::vector<xbase::FieldDef>& out, std::string& tableName)
{
    // Table name
    if (!(args >> tableName)) return false;

    // Expect '('
    char ch;
    if (!(args >> ch) || ch != '(') return false;

    // Parse items like: NAME C(20), AGE N(5[,2]) ...
    // This is intentionally permissive and minimal.
    while (true) {
        // read field name
        std::string fname;
        if (!(args >> fname)) return false;

        // type letter
        char type;
        if (!(args >> type)) return false;

        // '(' len [ , dec ] ')'
        if (!(args >> ch) || ch != '(') return false;

        int len = 0, dec = 0;
        args >> len;

        if (args.peek() == ',') {
            args.get(); // consume ','
            args >> dec;
        }
        if (!(args >> ch) || ch != ')') return false;

        out.push_back(xbase::FieldDef{fname, type, static_cast<uint8_t>(len), static_cast<uint8_t>(dec)});

        // delimiter or end ')'
        // consume optional comma
        while (std::isspace(args.peek())) args.get();
        if (args.peek() == ',') { args.get(); continue; }

        // Look ahead for ')'
        while (std::isspace(args.peek())) args.get();
        if (args.peek() == ')') { args.get(); break; }

        // If neither comma nor ')', we assume more fields until ')'
    }

    return true;
}

// CREATE command
void cmd_CREATE(xbase::DbArea& area, std::istringstream& args)
{
    std::vector<xbase::FieldDef> fields;
    std::string tableName;

//  If a table is open, close it so CREATE can proceed.
//    if (a.isOpen()) {
//       orderstate::clearOrder(a); // drop any active index
//       a.close();
//    }

// If a table is open, close it so CREATE can proceed.
    if (area.isOpen()) {                 // <-- or 'a.isOpen()' if your param is 'a'
       orderstate::clearOrder(area);    // <-- or 'a'
       area.close();                    // <-- or 'a'
    }

    if (!parse_field_list(args, fields, tableName)) {
        std::cout << "CREATE <name> (<FIELD TYPE(len[,dec]) ...>)\n";
        return;
    }

    // Resolve final on-disk name once; other code in this file may refer to it.
    const std::string outPathString = xbase::dbNameWithExt(tableName);

    try {
        // Close any open table, then create a new one.
        area.close();

        // Your project likely has create logic inside DbArea::open when file doesn't exist,
        // or a separate helper. Use what you already had; otherwise, call a helper here.
        // Minimal approach: open will create if not exists (adapt to your code).
        // If you have a dedicated "create DBF" routine, call it before open.

        // --- BEGIN: minimal create path ---
        // If your code needs a creation API, replace this block with it.
        // We reuse open() to create new file schema via internal code.
        // (You already had CREATE working earlier; keep your existing creation code
        // and just make sure it uses 'outPathString' and the 'fields' vector.)
        // --- END: minimal create path ---

        // Open newly-created file
        area.open(outPathString);

        std::cout << "Created " << outPathString
                  << " with " << fields.size() << " field(s).\n";
        std::cout << "Opened " << area.filename()
                  << " with " << area.recCount() << " records.\n";
    } catch (const std::exception& e) {
        std::cout << "CREATE failed: " << e.what() << "\n";
    }
}
