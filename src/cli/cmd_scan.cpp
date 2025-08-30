// cmd_scan.cpp â€” SCAN / ENDSCAN (simple FOR support; body executed per match)
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>

#include "xbase.hpp"
#include "textio.hpp"
#include "command_registry.hpp"
#include "scan_state.hpp"
#include "predicate_eval.hpp"   // predx::eval_expr(DbArea&, const std::string&)

using xbase::DbArea;

// Strip trailing '&& ...' outside quotes
static std::string strip_inline_comment(std::string s) {
    bool in_s = false, in_d = false;
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        char c = s[i], n = s[i+1];
        if (!in_s && c == '"')  in_d = !in_d;
        else if (!in_d && c=='\'') in_s = !in_s;
        if (!in_s && !in_d && c=='&' && n=='&') { s.erase(i); break; }
    }
    return textio::rtrim(s);
}

// SCAN [FOR <expr>]
void cmd_SCAN(DbArea&, std::istringstream& iss) {
    auto& st = scanblock::state();
    st.active = true;
    st.lines.clear();
    st.for_expr.reset();

    // Optional: FOR <expr...>
    std::streampos save = iss.tellg();
    std::string maybe;
    if (iss >> maybe) {
        if (textio::ieq(maybe, "FOR")) {
            std::string rest;
            std::getline(iss, rest);
            rest = textio::trim(strip_inline_comment(rest));
            if (!rest.empty()) st.for_expr = rest;
        } else {
            iss.clear(); iss.seekg(save);
        }
    }

    std::cout << "SCAN: buffering lines. Type ENDSCAN to execute"
              << (st.for_expr ? " (FOR present)." : ".") << "\n";
}

// ENDSCAN â€” execute buffered body for each matching record
void cmd_ENDSCAN(DbArea& A, std::istringstream&) {
    auto& st = scanblock::state();

    if (!st.active) { std::cout << "No active SCAN.\n"; return; }
    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        st.active = false; st.lines.clear(); st.for_expr.reset();
        return;
    }

    long long matched = 0, iters = 0;

    // Start at current record (FoxPro-compatible). If invalid, try TOP.
    bool pos_ok = A.readCurrent();
    if (!pos_ok) pos_ok = (A.top() && A.readCurrent());

    if (pos_ok) {
        do {
            if (A.isDeleted()) continue;

            bool ok = true;
            if (st.for_expr) ok = predx::eval_expr(A, *st.for_expr);
            if (!ok) continue;

            ++matched;

            // Execute the buffered body lines for this record
            for (auto raw : st.lines) {
                raw = textio::trim(strip_inline_comment(raw));
                if (raw.empty()) continue;

                std::istringstream iss(raw);
                std::string cmd; iss >> cmd;
                if (cmd.empty()) continue;

                const std::string U = textio::up(cmd);
                if (U == "SCAN" || U == "ENDSCAN") continue; // ignore nested headers

                if (!dli::registry().run(A, U, iss)) {
                    std::cout << "Unknown command in SCAN: " << cmd << "\n";
                }
            }

            ++iters;
        } while (A.skip(+1) && A.readCurrent());
    }

    st.active = false;
    st.lines.clear();
    st.for_expr.reset();

    std::cout << "ENDSCAN: " << matched << " match(es), " << iters << " iteration(s).\n";
}

// Self-register
static bool s_reg = [](){
    dli::registry().add("SCAN",    [](DbArea& a, std::istringstream& s){ cmd_SCAN(a,s); });
    dli::registry().add("ENDSCAN", [](DbArea& a, std::istringstream& s){ cmd_ENDSCAN(a,s); });
    return true;
}();

