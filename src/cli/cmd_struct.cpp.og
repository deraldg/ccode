// src/cli/cmd_struct.cpp
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "xbase.hpp"
#include "order_state.hpp"

using xbase::DbArea;
using xbase::FieldDef;

static int digits(std::size_t n) {
    int d = 1;
    while (n >= 10) { n /= 10; ++d; }
    return d;
}

void cmd_STRUCT(DbArea& a, std::istringstream&) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    const auto& defs = a.fields();
    std::size_t n = defs.size();

    // Header
    std::cout << "Table: " << a.name()
              << "   Fields: " << n
              << "   Record length: " << a.cpr() << " bytes\n";

    try {
        bool asc = orderstate::isAscending(a);
        std::string tag = orderstate::hasOrder(a) ? orderstate::orderName(a) : std::string("(none)");
        std::cout << "Active order: TAG " << tag << " (" << (asc ? "ASCEND" : "DESCEND") << ")\n";
    } catch (...) { /* ignore if not available */ }

    // Column widths
    int w_idx  = digits(n);
    int w_name = 12;
    int w_type = 4;
    int w_len  = 4;
    int w_dec  = 3;
    std::cout << "\n"
              << std::right << std::setw(w_idx)  << "#" << ' '
              << std::left  << std::setw(w_name) << "NAME" << ' '
              << std::left  << std::setw(w_type) << "TYPE" << ' '
              << std::right << std::setw(w_len)  << "LEN" << ' '
              << std::right << std::setw(w_dec)  << "DEC" << "\n";

    auto dash = [](int w){ for (int i=0;i<w;++i) std::cout << '-'; };
    dash(w_idx); std::cout << ' ';
    dash(w_name); std::cout << ' ';
    dash(w_type); std::cout << ' ';
    dash(w_len);  std::cout << ' ';
    dash(w_dec);  std::cout << "\n";

    for (std::size_t i = 0; i < n; ++i) {
        const FieldDef& f = defs[i];
        std::cout
            << std::right << std::setw(w_idx)  << (i+1) << ' '
            << std::left  << std::setw(w_name) << f.name << ' '
            << std::left  << std::setw(w_type) << f.type << ' '
            << std::right << std::setw(w_len)  << static_cast<int>(f.length) << ' '
            << std::right << std::setw(w_dec)  << static_cast<int>(f.decimals)
            << "\n";
    }
}
