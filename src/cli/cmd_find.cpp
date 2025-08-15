#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "xbase.hpp"
#include "textio.hpp"
#include "predicates.hpp"

// FIND <field> <needle>  (needle may be quoted) — case-insensitive substring
void cmd_FIND(xbase::DbArea& area, std::istringstream& iss)
{
    if (!area.isOpen()) { std::cout << "No table open.\n"; return; }

    // parse args from the rest of the line
    std::string rest((std::istreambuf_iterator<char>(iss)), std::istreambuf_iterator<char>());
    rest = textio::trim(rest);
    auto args = textio::tokenize(rest);

    if (args.size() < 2) {
        std::cout << "Usage: FIND <field> <needle>\n";
        return;
    }

    const std::string fld    = args[0];
    const std::string needle = textio::unquote(args[1]);

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
    const std::string needle_lc = tolc(needle);

    if (!area.top()) { std::cout << "Empty table.\n"; return; }

    bool any = false;
    do {
        if (!area.readCurrent()) continue;
        const std::string v_lc = tolc(area.get(fidx));
        if (!needle_lc.empty() && v_lc.find(needle_lc) != std::string::npos) {
            any = true;
            std::cout << area.recno() << ": " << area.get(fidx) << "\n";
        }
    } while (area.skip(1));

    if (!any) std::cout << "No matches.\n";
}
