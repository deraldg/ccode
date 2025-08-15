#include <iostream>
#include <sstream>
#include "xbase.hpp"

using namespace xbase;

void cmd_APPEND(DbArea& a, std::istringstream& iss) {
    (void)iss;
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    if (!a.appendBlank()) { std::cout << "Append failed.\n"; return; }
    std::cout << "Appending record " << a.recno() << ". Enter values or ENTER to keep blank.\n";
    for (int i = 1; i <= a.fieldCount(); ++i) {
        const auto& f = a.fields()[static_cast<size_t>(i - 1)];
        std::cout << "  " << f.name << " (" << int(f.length) << "): ";
        std::string v; std::getline(std::cin, v);
        if (!v.empty()) { if (v.size() > f.length) v.resize(f.length); a.set(i, v); }
    }
    if (a.writeCurrent()) std::cout << "Record written.\n";
    else std::cout << "Write failed.\n";
}
