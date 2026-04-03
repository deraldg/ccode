#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "workareas.hpp"

void cmd_GPS(xbase::DbArea& current, std::istringstream& /*iss*/)
{
    const std::size_t cur_area = workareas::current_slot();
    const std::size_t area_count = workareas::count();

    if (!current.isOpen()) {
        std::cout
            << "Cursor: Area " << cur_area
            << " of " << (area_count ? (area_count - 1) : 0)
            << " ... No table open\n";
        return;
    }

    int recno = 0;
    try {
        recno = current.recno();
    } catch (...) {
        recno = 0;
    }

    int logical_row = recno;   // v1 fallback until unified logical-row API is wired

    std::string table_name;
    try {
        table_name = workareas::current().label();
    } catch (...) {
        table_name.clear();
    }

    if (table_name.empty()) {
        table_name = "(unnamed)";
    }

    std::cout
        << "Cursor: Area " << cur_area
        << " of " << (area_count ? (area_count - 1) : 0)
        << " ... Table " << table_name
        << " ... Physical Recno " << recno
        << ", Logical Row " << logical_row
        << "\n";
}