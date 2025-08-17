// src/cli/cmd_count.cpp
#include "xbase.hpp"
#include "predicates.hpp"
#include "textio.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

namespace {

struct Opts {
    enum Mode { SkipDeleted, IncludeDeleted, OnlyDeleted } mode{SkipDeleted};
    bool haveFilter{false};
    std::string fld, op, val;
};

Opts parse_opts(std::istringstream& iss) {
    Opts o{};
    std::string tok;

    // Optional first token: ALL or DELETED
    std::streampos save = iss.tellg();
    if (iss >> tok) {
        if (textio::ieq(tok, "ALL"))      o.mode = Opts::IncludeDeleted;
        else if (textio::ieq(tok, "DELETED")) o.mode = Opts::OnlyDeleted;
        else { iss.clear(); iss.seekg(save); }
    }

    // Optional: FOR <fld> <op> <value...>
    save = iss.tellg();
    std::string forWord;
    if (iss >> forWord) {
        if (textio::ieq(forWord, "FOR")) {
            if (iss >> o.fld >> o.op) {
                std::string rest;
                std::getline(iss, rest);
                o.val = textio::trim(rest);
                o.haveFilter = !o.fld.empty() && !o.op.empty() && !o.val.empty();
            }
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }
    return o;
}

} // namespace

void cmd_COUNT(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    Opts opt = parse_opts(iss);

    const int32_t total = a.recCount();
    if (total <= 0) { std::cout << 0 << "\n"; return; }

    if (a.recno() <= 0) a.top();

    int64_t cnt = 0;
    for (int32_t rn = 1; rn <= total; ++rn) {
        if (!a.gotoRec(rn)) break;
        if (!a.readCurrent()) continue;

        const bool del = a.isDeleted();
        if (opt.mode == Opts::SkipDeleted && del) continue;
        if (opt.mode == Opts::OnlyDeleted && !del) continue;

        if (opt.haveFilter && !predicates::eval(a, opt.fld, opt.op, opt.val))
            continue;

        ++cnt;
    }

    std::cout << cnt << "\n";
}
