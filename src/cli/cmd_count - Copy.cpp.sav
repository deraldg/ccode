// COUNT [ALL|DELETED] [FOR expr] [WHILE expr] [NEXT n|RECORD n|REST]
#include <iostream>
#include <sstream>
#include "xbase.hpp"
#include "parse.hpp"
#include "scan.hpp"

using xbase::DbArea;

static void print_usage() {
    std::cout << "Usage: COUNT [ALL|DELETED] [FOR <expr>] [WHILE <expr>] [NEXT n|RECORD n|REST]\n";
}

void cmd_COUNT(DbArea& A, std::istringstream& S) {
    auto pr = parse_scan_options(S, "COUNT");
    if (!pr.ok) { print_usage(); if (!pr.err.empty()) std::cout << pr.err << "\n"; return; }
    if (!A.isOpen() || A.recCount() <= 0) { std::cout << "0\n"; return; }

    int cnt = 0;
    scan_records(A, pr.opt, [&](DbArea& /*cur*/) {
        ++cnt;                  // count every record that passes FOR/WHILE/DELETED filters
        return true;            // keep scanning
    });

    std::cout << cnt << "\n";
}
