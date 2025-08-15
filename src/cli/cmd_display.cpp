#include <iostream>
#include <sstream>
#include <fstream>
#include "xbase.hpp"

static bool current_is_deleted(const xbase::DbArea& a) {
    if (!a.recno()) return false;
    std::ifstream in(a.name(), std::ios::binary);
    if (!in) return false;
    xbase::HeaderRec hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;
    std::streampos pos = hdr.data_start + static_cast<std::streamoff>((a.recno()-1) * a.cpr());
    in.seekg(pos, std::ios::beg);
    char flag = ' ';
    in.read(&flag, 1);
    return in && flag == xbase::IS_DELETED;
}

void cmd_DISPLAY(xbase::DbArea& a, std::istringstream& iss) {
    (void)iss;
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }
    if (!a.recno())  { std::cout << "No current record\n"; return; }
    const auto& fds = a.fields();
    bool del = current_is_deleted(a);
    std::cout << "Record " << a.recno() << (del ? " [DELETED]" : "") << "\n";
    for (int i=1; i<=a.fieldCount(); ++i) {
        const auto& f = fds[static_cast<size_t>(i-1)];
        std::cout << "  " << f.name << " = " << a.get(i) << "\n";
    }
}
