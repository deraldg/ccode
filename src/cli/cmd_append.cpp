// src/cli/cmd_append.cpp  (drop-in replacement, hardened)
// Consolidates APPEND and APPEND_BLANK behavior and guards bad input.
//
// Supports:
//   APPEND                         -> append 1 blank record
//   APPEND BLANK                   -> append 1 blank record
//   APPEND -B                      -> append 1 blank record
//   APPEND N                       -> append N blank records
//   APPEND BLANK N | APPEND -B N   -> append N blank records
//
// Errors out (no crash) on garbage like: APPEND FROG, APPEND -B X, APPEND -7
// Uses DbArea::appendBlank(). Calls order_notify_mutation(db) after success.

#include "xbase.hpp"
#include "textio.hpp"
#include "order_hooks.hpp"
#include <sstream>
#include <string>
#include <cctype>
#include <iostream>

using namespace xbase;

namespace {

inline std::string uptrim(std::string s) {
    return textio::up(textio::trim(s));
}

inline bool try_parse_int_strict(const std::string& s, int& out) {
    if (s.empty()) return false;
    // Only digits, positive
    for (char c : s) if (c < '0' || c > '9') return false;
    try {
        out = std::stoi(s);
        return out > 0;
    } catch (...) { return false; }
}

// Append N blank records; return false if any append fails
static bool append_n_blank(DbArea& db, int n) {
    if (n <= 0) return true;
    for (int i = 0; i < n; ++i) {
        if (!db.appendBlank()) return false;
    }
    return true;
}

} // namespace

// APPEND [BLANK|-B] [n]
void cmd_APPEND(DbArea& db, std::istringstream& iss) {
    std::string t1; // first token after APPEND
    std::string t2; // optional second token (n)

    // read tokens loosely
    iss >> t1;
    iss >> t2;

    const std::string a1 = uptrim(t1);
    const std::string a2 = uptrim(t2);

    int  count   = 1;     // default number of records
    bool doBlank = true;  // classic drop-through

    // Validate / interpret args
    if (a1.empty()) {
        // APPEND -> BLANK 1
        doBlank = true;
        count   = 1;
    } else if (a1 == "BLANK" || a1 == "-B") {
        // APPEND BLANK [n]  or  APPEND -B [n]
        if (!a2.empty()) {
            int n{};
            if (!try_parse_int_strict(a2, n)) {
                std::cout << "Usage: APPEND [BLANK|-B] [n]\n";
                return;
            }
            count = n;
        }
        doBlank = true;
    } else {
        // APPEND N  (N must be positive integer)
        int n{};
        if (!try_parse_int_strict(a1, n)) {
            std::cout << "Usage: APPEND [BLANK|-B] [n]\n";
            return;
        }
        count   = n;
        doBlank = true;
    }

    // Execute
    try {
        if (doBlank) {
            if (!append_n_blank(db, count)) {
                std::cout << "APPEND failed.\n";
                return;
            }
            // Safe no-op by default; plug in real notifier later.
            order_notify_mutation(db);
            std::cout << "Appended " << count
                      << " blank record" << (count == 1 ? "" : "s")
                      << ".\n";
            return;
        }
    } catch (...) {
        std::cout << "APPEND failed (exception).\n";
        return;
    }

    std::cout << "APPEND: interactive mode not yet implemented.\n";
}
