// ===============================
// File: src/cli/cmd_setorder.cpp  (DROP-IN)
// ===============================
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "textio.hpp"
#include "xindex/index_manager.hpp"
#include "xindex/attach.hpp"

static bool is_number(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) if (!std::isdigit(c)) return false;
    return true;
}

void cmd_SETORDER(xbase::DbArea& a, std::istringstream& args) {
    using std::cout; using std::endl; using std::string; using std::vector;

    if (!a.isOpen()) { cout << "SET ORDER: no file open." << endl; return; }

    string tok;
    if (!(args >> tok)) { cout << "Usage: SETORDER <n|tag>" << endl; return; }

    auto& mgr = xindex::ensure_manager(a);
    vector<string> tags = mgr.listTags();
    if (tags.empty()) { cout << "SET ORDER: no indexes loaded." << endl; return; }

    string chosen;
    if (is_number(tok)) {
        std::sort(tags.begin(), tags.end());
        int n = std::stoi(tok);
        if (n < 1 || n > (int)tags.size()) {
            cout << "SET ORDER: index not found: " << tok << endl; return;
        }
        chosen = tags[n-1];
    } else {
        // direct tag name
        auto it = std::find(tags.begin(), tags.end(), tok);
        if (it == tags.end()) { cout << "SET ORDER: index not found: " << tok << endl; return; }
        chosen = tok;
    }

    if (!mgr.set_active(chosen)) {
        cout << "SET ORDER: failed to activate tag: " << chosen << endl; return;
    }

    cout << "Order set: tag '" << chosen << "'" << endl;
}

