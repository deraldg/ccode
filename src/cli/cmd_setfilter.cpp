
#include "xbase.hpp"
#include "filters/filter_registry.hpp"
#include <sstream>
#include <iostream>

void cmd_SETFILTER(xbase::DbArea& area, std::istringstream& args) {
    std::string first;
    if (!(args >> first)) {
        std::cout << "SET FILTER TO <expr> | SET FILTER TO (clear)\n";
        return;
    }
    for (auto& c : first) c = (char)std::toupper((unsigned char)c);
    if (first != "TO") {
        std::cout << "SET FILTER: expected 'TO'.\n";
        return;
    }

    std::string expr;
    std::getline(args, expr);
    if (!expr.empty() && expr.front()==' ') expr.erase(0,1);

    if (expr.empty()) {
        filter::clear(&area);
        std::cout << "SET FILTER: cleared.\n";
        return;
    }

    std::string err;
    if (!filter::set(&area, expr, err)) {
        std::cout << "SET FILTER: error: " << err << "\n";
        return;
    }
    std::cout << "SET FILTER TO " << expr << "\n";
}



