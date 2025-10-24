#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include "xbase.hpp"
#include "textio.hpp"
#include "predicates.hpp"

namespace {

bool read_header(const std::string& path, xbase::HeaderRec& hdr) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    return static_cast<bool>(in);
}

bool recall_record_at(std::fstream& io, const xbase::HeaderRec& hdr, int recno) {
    if (recno < 1 || recno > hdr.num_of_recs) return false;
    std::streampos pos = hdr.data_start + static_cast<std::streamoff>((recno-1) * hdr.cpr);
    io.seekp(pos, std::ios::beg);
    char flag = xbase::NOT_DELETED;
    io.write(&flag, 1);
    return static_cast<bool>(io);
}

} // anon

void cmd_RECALL(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No file open\n"; return; }

    std::string token;
    std::string forField, forOp, forVal;
    std::string whileField, whileOp, whileVal;
    bool all = false;

    std::vector<std::string> toks;
    while (iss >> token) toks.push_back(token);

    for (size_t i = 0; i < toks.size(); ) {
        std::string t = textio::up(toks[i]);
        if (t == "FOR" && i + 3 <= toks.size() - 1) {
            forField = toks[i+1]; forOp = toks[i+2]; forVal = toks[i+3]; i += 4;
        } else if (t == "WHILE" && i + 3 <= toks.size() - 1) {
            whileField = toks[i+1]; whileOp = toks[i+2]; whileVal = toks[i+3]; i += 4;
        } else if (t == "ALL") {
            all = true; ++i;
        } else { ++i; }
    }

    xbase::HeaderRec hdr{};
    if (!read_header(a.name(), hdr)) { std::cout << "Failed reading header.\n"; return; }

    std::fstream io(a.name(), std::ios::in | std::ios::out | std::ios::binary);
    if (!io) { std::cout << "Cannot open file for update.\n"; return; }

    int recalled = 0;

    auto try_recall = [&](int r){
        if (!a.gotoRec(r)) return;
        std::streampos pos = hdr.data_start + static_cast<std::streamoff>((r-1) * hdr.cpr);
        io.seekg(pos, std::ios::beg);
        char flag = 0; io.read(&flag, 1);
        if (!io) return;
        if (flag == xbase::IS_DELETED) {
            if (recall_record_at(io, hdr, r)) ++recalled;
        }
    };

    if (forField.empty() && whileField.empty() && !all) {
        int r = a.recno();
        if (!r) { std::cout << "No current record.\n"; return; }
        try_recall(r);
    } else {
        int start = all ? 1 : (a.recno() ? a.recno() : 1);
        for (int r = start; r <= a.recCount(); ++r) {
            if (!a.gotoRec(r)) break;
            if (!whileField.empty() && !predicates::eval(a, whileField, whileOp, whileVal)) break;
            if (forField.empty() || predicates::eval(a, forField, forOp, forVal))
                try_recall(r);
        }
    }

    if (a.recno()) a.gotoRec(a.recno());
    std::cout << "Recalled " << recalled << " record(s).\n";
}
