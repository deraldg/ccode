// src/cli/cmd_go.cpp — GO
//
// Supported forms:
//   GO TOP
//   GO BOTTOM
//   GO [TO] n         (e.g., GO 10, GO TO 10)
//   GO RECORD n
//
// Behavior:
//   • Silent on success (FoxPro-style).
//   • Prints concise errors on bad input.
//   • Delegates to engine: top(), bottom(), gotoRec(n).
//
// Registrar example (shell.cpp):
//   void cmd_GO(xbase::DbArea&, std::istringstream&);
//   registry.add("GO", &cmd_GO);

#include <sstream>
#include <string>
#include <cctype>
#include <iostream>
#include <limits>
#include <algorithm>

#include "xbase.hpp"
#include "textio.hpp"

using namespace textio;

static inline std::string upcopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

// Parse integer with optional leading +/-
static bool try_parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i == s.size()) return false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    }
    try {
        long long v = std::stoll(s);
        if (v < static_cast<long long>(std::numeric_limits<int>::min()) ||
            v > static_cast<long long>(std::numeric_limits<int>::max()))
            return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) { return false; }
}

void cmd_GO(xbase::DbArea& A, std::istringstream& in) {
    std::string tok;
    if (!(in >> tok)) {
        std::cout << "GO: expected TOP, BOTTOM, TO n, RECORD n, or a record number.\n";
        return;
    }
    const std::string U = upcopy(tok);

    try {
        if (U == "TOP")    { A.top();    return; }
        if (U == "BOTTOM") { A.bottom(); return; }

        if (U == "TO") {
            std::string nTok;
            if (!(in >> nTok)) { std::cout << "GO: expected a record number after TO.\n"; return; }
            int n;
            if (!try_parse_int(nTok, n) || n <= 0) { std::cout << "GO: record number must be a positive integer.\n"; return; }
            A.gotoRec(n);
            return;
        }

        if (U == "RECORD") {
            std::string nTok;
            if (!(in >> nTok)) { std::cout << "GO: expected a record number after RECORD.\n"; return; }
            int n;
            if (!try_parse_int(nTok, n) || n <= 0) { std::cout << "GO: record number must be a positive integer.\n"; return; }
            A.gotoRec(n);
            return;
        }

        // GO <n>
        int n;
        if (try_parse_int(tok, n) && n > 0) {
            A.gotoRec(n);
            return;
        }

        if (U == "IN") {
            std::cout << "GO: 'IN <alias>' not supported yet (SELECT the area, then GO ...).\n";
            return;
        }

        std::cout << "GO: unrecognized form. Use TOP, BOTTOM, TO n, RECORD n, or a number.\n";
    } catch (const std::exception& e) {
        std::cout << "GO: " << e.what() << "\n";
    } catch (...) {
        std::cout << "GO: unexpected error.\n";
    }
}
