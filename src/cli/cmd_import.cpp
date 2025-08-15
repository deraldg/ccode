#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "xbase.hpp"
#include "csv.hpp"
#include "textio.hpp"
#include "predicates.hpp"

using namespace xbase;

void cmd_IMPORT(DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    std::string csvfile; iss >> csvfile;
    if (csvfile.empty()) { std::cout << "Usage: IMPORT <csvfile>\n"; return; }
    if (!textio::ends_with_ci(csvfile, ".csv")) csvfile += ".csv";

    std::ifstream in(csvfile, std::ios::binary);
    if (!in) { std::cout << "Cannot open " << csvfile << " for read.\n"; return; }

    std::string line;
    if (!std::getline(in, line)) { std::cout << "Empty CSV.\n"; return; }
    auto headers = csv::split_line(line);

    std::vector<int> col2fld; col2fld.reserve(headers.size());
    for (auto &h : headers)
        col2fld.push_back(predicates::field_index_ci(a, textio::trim(h)));

    int imported = 0;
    while (std::getline(in, line)) {
        auto cols = csv::split_line(line);
        if (cols.empty()) continue;
        if (!a.appendBlank()) { std::cout << "Append failed.\n"; break; }
        for (size_t c = 0; c < cols.size() && c < col2fld.size(); ++c) {
            int fi = col2fld[c];
            if (fi > 0) a.set(fi, cols[c]);
        }
        if (!a.writeCurrent()) { std::cout << "Write failed at rec " << a.recno() << "\n"; break; }
        ++imported;
    }
    std::cout << "Imported " << imported << " records from " << csvfile << "\n";
}
