// src/cli/cmd_export.cpp
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "csv.hpp"
#include "textio.hpp"

using namespace xbase;

static bool ieq_to(const std::string& s) {
    if (s.size() != 2) return false;
    return (s[0] == 'T' || s[0] == 't') && (s[1] == 'O' || s[1] == 'o');
}

void cmd_EXPORT(DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }

    // Accept either:
    //   EXPORT <file>
    // or
    //   EXPORT <table> TO <file>
    std::string tok1; if (!(iss >> tok1)) {
        std::cout << "Usage: EXPORT <table> TO <file>\n"; return;
    }

    std::string dest;
    std::string tok2;
    std::streampos savePos = iss.tellg();
    if (iss >> tok2) {
        if (ieq_to(tok2)) {
            // form: <table> TO <file>
            if (!(iss >> dest) || dest.empty()) {
                std::cout << "Usage: EXPORT <table> TO <file>\n"; return;
            }
        } else {
            // fallback to single-arg form: first token is the file name
            dest = tok1;
            iss.seekg(savePos); // put back tok2 for safety (not used)
        }
    } else {
        dest = tok1;
    }

    if (!textio::ends_with_ci(dest, ".csv")) dest += ".csv";

    std::ofstream out(dest, std::ios::binary);
    if (!out) { std::cout << "Unable to open " << dest << " for write\n"; return; }

    // header
    const auto& Fs = a.fields();
    for (int i = 1; i <= a.fieldCount(); ++i) {
        if (i > 1) out << ",";
        out << csv::escape(Fs[static_cast<size_t>(i - 1)].name);
    }
    out << "\n";

    // rows (natural table order)
    for (int r = 1; r <= a.recCount(); ++r) {
        if (!a.gotoRec(r) || !a.readCurrent()) continue;
        for (int i = 1; i <= a.fieldCount(); ++i) {
            if (i > 1) out << ",";
            out << csv::escape(a.get(i));
        }
        out << "\n";
    }

    std::cout << "Exported " << a.recCount() << " records to " << dest << "\n";
}
