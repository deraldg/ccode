#include <iostream>
#include <sstream>
#include "xbase.hpp"

using namespace xbase;

void cmd_APPEND_BLANK(DbArea& A, std::istringstream& S) {
    int n = 1; S >> n; if (n <= 0) n = 1;
    int before = A.recCount();
    for (int i=0;i<n;++i) A.appendBlank();   // your engine call
    // position on the last appended record and ensure buffer is loaded
    A.gotoRec(A.recCount());
    A.readCurrent();
    std::cout << "Appended " << n << " blank record(s). New count: " << A.recCount() << ".\n";
}

