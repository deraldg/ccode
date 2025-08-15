#include <fstream>
#include <iostream>
#include <sstream>
#include "xbase.hpp"
#include "csv.hpp"
#include "textio.hpp"

using namespace xbase;

void cmd_EXPORT(DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    std::string csvfile; iss >> csvfile;
    if (csvfile.empty()) { std::cout << "Usage: EXPORT <csvfile>\n"; return; }
    if (!textio::ends_with_ci(csvfile, ".csv")) csvfile += ".csv";

    std::ofstream out(csvfile, std::ios::binary);
    if (!out) { std::cout << "Cannot open " << csvfile << " for write.\n"; return; }

    for (int i = 0; i < a.fieldCount(); ++i) {
        if (i) out << ",";
        out << csv::escape(a.fields()[static_cast<size_t>(i)].name);
    }
    out << "\n";

    for (int r = 1; r <= a.recCount(); ++r) {
        if (!a.gotoRec(r)) break;
        for (int i = 1; i <= a.fieldCount(); ++i) {
            if (i > 1) out << ",";
            out << csv::escape(a.get(i));
        }
        out << "\n";
    }
    std::cout << "Exported " << a.recCount() << " records to " << csvfile << "\n";
}
