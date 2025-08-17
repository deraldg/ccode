#include "xbase.hpp"
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

using xbase::DbArea;
using xbase::FieldDef;

namespace {

// compute the width in digits for numbering column (“#”)
static int digits(std::size_t n) {
    int d = 1;
    while (n >= 10) { n /= 10; ++d; }
    return d;
}

} // namespace

// FIELDS
// Shows the field list with aligned columns.
void cmd_FIELDS(DbArea& area, std::istringstream& /*unused*/)
{
    const auto& defs = area.fields();
    const std::size_t n = static_cast<std::size_t>(area.fieldCount());

    if (n == 0) {
        std::cout << "(No fields)\n";
        return;
    }

    // Column widths
    int w_idx  = std::max(1, digits(n));               // '#'
    int w_name = 4;                                     // 'Name'
    int w_type = 4;                                     // 'Type'
    int w_len  = 3;                                     // 'Len'

    for (const FieldDef& f : defs) {
        w_name = std::max<int>(w_name, static_cast<int>(f.name.size()));
        w_type = std::max<int>(w_type, 1);              // single letter
        w_len  = std::max<int>(w_len,  (f.length > 0 ? (f.length < 10 ? 1 : (f.length < 100 ? 2 : 3)) : 1));
    }

    // Header
    std::cout
        << std::right << std::setw(w_idx)  << "#"  << ' '
        << std::left  << std::setw(w_name) << "Name" << ' '
        << std::left  << std::setw(w_type) << "Type" << ' '
        << std::right << std::setw(w_len)  << "Len"  << "\n";

    // Ruler
    auto dash = [](int w){ for (int i=0;i<w;++i) std::cout << '-'; };
    dash(w_idx);  std::cout << ' ';
    dash(w_name); std::cout << ' ';
    dash(w_type); std::cout << ' ';
    dash(w_len);  std::cout << "\n";

    // Rows
    for (std::size_t i = 0; i < n; ++i) {
        const FieldDef& f = defs[i];
        std::cout
            << std::right << std::setw(w_idx)  << (i+1) << ' '
            << std::left  << std::setw(w_name) << f.name << ' '
            << std::left  << std::setw(w_type) << f.type << ' '
            << std::right << std::setw(w_len)  << static_cast<int>(f.length)
            << "\n";
    }
}
