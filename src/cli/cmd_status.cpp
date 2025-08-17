// src/cli/cmd_status.cpp
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include "xbase.hpp"

using xbase::HeaderRec;
using xbase::IS_DELETED;

static bool current_is_deleted(const xbase::DbArea& a) {
    if (!a.isOpen() || !a.recno()) return false;
    std::ifstream in(a.name(), std::ios::binary);
    if (!in) return false;
    HeaderRec hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) return false;
    std::streampos pos = hdr.data_start + static_cast<std::streamoff>((a.recno()-1) * a.cpr());
    in.seekg(pos, std::ios::beg);
    char flag = ' ';
    in.read(&flag, 1);
    return in && flag == IS_DELETED;
}

void cmd_STATUS(xbase::DbArea& a, std::istringstream& iss) {
    (void)iss;
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }

    std::ifstream in(a.name(), std::ios::binary);
    if (!in) { std::cout << "Failed to read header\n"; return; }

    HeaderRec hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!in) { std::cout << "Failed to read header\n"; return; }

    std::cout << "File:        " << a.name()     << "\n";
    std::cout << "Records:     " << a.recCount() << "\n";
    std::cout << "Current:     " << a.recno()    << (current_is_deleted(a) ? " [DELETED]\n" : "\n");
    std::cout << "Bytes/rec:   " << a.cpr()      << "\n";
    std::cout << "Data start:  " << hdr.data_start << "\n";
}
