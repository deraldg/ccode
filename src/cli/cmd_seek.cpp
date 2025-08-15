#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "xbase.hpp"
#include "textio.hpp"
#include "predicates.hpp"

// SEEK <field> <value>  (value may be quoted) — case-insensitive exact match
void cmd_SEEK(xbase::DbArea& area, std::istringstream& iss)
{
    if (!area.isOpen()) { std::cout << "No table open.\n"; return; }

    // parse args from the rest of the line
    std::string rest((std::istreambuf_iterator<char>(iss)), std::istreambuf_iterator<char>());
    rest = textio::trim(rest);
    auto args = textio::tokenize(rest);

    if (args.size() < 2) {
        std::cout << "Usage: SEEK <field> <value>\n";
        return;
    }

    const std::string fld   = args[0];
    const std::string value = textio::unquote(args[1]);

    int fidx = predicates::field_index_ci(area, fld);
    if (fidx <= 0) {
        std::cout << "Unknown field: " << fld << "\n";
        return;
    }

    auto tolc = [](std::string s){
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return s;
    };
    const std::string value_lc = tolc(value);

    if (!area.top()) { std::cout << "Empty table.\n"; return; }

    do {
        if (!area.readCurrent()) continue;
        if (tolc(area.get(fidx)) == value_lc) {
            std::cout << "Found at " << area.recno() << ".\n";
            return;
        }
    } while (area.skip(1));

    std::cout << "Not found.\n";
}
