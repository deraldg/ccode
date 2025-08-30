#include <fstream>
#include <iostream>
#include <sstream>
#include "xbase.hpp"
#include "textio.hpp"

using namespace xbase;

void cmd_COPY(DbArea& a, std::istringstream& iss) {
    std::string t1; iss >> t1;
    if (t1.empty()) { std::cout << "Usage: COPY <DBFNAME> | COPY TO <DBFNAME>\n"; return; }
    if (textio::up(t1) == "TO") {
        std::string dst; iss >> dst;
        if (dst.empty()) { std::cout << "Usage: COPY TO <DBFNAME>\n"; return; }
        if (!a.isOpen()) { std::cout << "No file open\n"; return; }
        std::string src = a.name();
        if (!textio::ends_with_ci(dst, ".dbf")) dst += ".dbf";
        std::ifstream ifs(src, std::ios::binary);
        std::ofstream ofs(dst, std::ios::binary);
        ofs << ifs.rdbuf();
        if (ofs.good()) std::cout << "Copied to " << dst << "\n";
        else std::cout << "Copy failed.\n";
    } else {
        std::string db = t1;
        if (!textio::ends_with_ci(db, ".dbf")) db += ".dbf";
        try {
            a.open(db);
            std::cout << "Opened " << db << " with " << a.recCount() << " records.\n";
        } catch (const std::exception& e) {
            std::cout << "Open failed: " << e.what() << "\n";
        }
    }
}
