#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cli/logical_nav.hpp"
#include "cli/settings.hpp"

namespace cli::nav {

enum class Endpoint {
    Top,
    Bottom,
    First,
    Last
};

inline std::string upper_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

inline bool try_parse_int_token(const std::string& s, int& out)
{
    if (s.empty()) return false;

    std::size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i == s.size()) return false;

    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }

    try {
        const long long v = std::stoll(s);
        if (v < static_cast<long long>(std::numeric_limits<int>::min()) ||
            v > static_cast<long long>(std::numeric_limits<int>::max())) {
            return false;
        }
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool refresh_current(xbase::DbArea& A, const char* who)
{
    if (!A.isOpen()) {
        std::cout << who << ": no file open.\n";
        return false;
    }
    if (!A.readCurrent()) {
        std::cout << who << ": failed to read record.\n";
        return false;
    }
    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
    return true;
}

inline bool go_absolute(xbase::DbArea& A, int n, const char* who)
{
    if (!A.isOpen()) {
        std::cout << who << ": no file open.\n";
        return false;
    }
    if (n <= 0) {
        std::cout << who << ": record number must be a positive integer.\n";
        return false;
    }
    if (!A.gotoRec(n) || !A.readCurrent()) {
        std::cout << who << ": failed.\n";
        return false;
    }
    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
    return true;
}

inline bool go_endpoint(xbase::DbArea& A, Endpoint ep, const char* who)
{
    if (!A.isOpen()) {
        std::cout << who << ": no file open.\n";
        return false;
    }

    int32_t rn = 0;

    switch (ep) {
    case Endpoint::Top:
    case Endpoint::First:
        rn = cli::logical_nav::first_recno(A);
        break;

    case Endpoint::Bottom:
    case Endpoint::Last:
        rn = cli::logical_nav::last_recno(A);
        break;
    }

    if (rn <= 0) {
        std::cout << who << ": failed.\n";
        return false;
    }

    if (!A.gotoRec(rn) || !A.readCurrent()) {
        std::cout << who << ": failed.\n";
        return false;
    }

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Recno: " << A.recno() << "\n";
    }
    return true;
}

inline bool skip_relative(xbase::DbArea& A, int n, const char* who)
{
    if (!A.isOpen()) {
        std::cout << who << ": no file open.\n";
        return false;
    }

    const bool talk = cli::Settings::instance().talk_on.load();

    int steps = (n >= 0 ? n : -n);
    const bool forward = (n >= 0);

    if (steps == 0) {
        if (!A.readCurrent()) {
            std::cout << who << ": failed to read record.\n";
            return false;
        }
        if (talk) std::cout << "Recno: " << A.recno() << "\n";
        return true;
    }

    int32_t current = A.recno();
    int32_t next = 0;
    bool moved = false;

    while (steps-- > 0) {
        next = forward
            ? cli::logical_nav::next_recno(A, current)
            : cli::logical_nav::prev_recno(A, current);

        if (next <= 0) {
            if (!moved) {
                std::cout << who << ": at end.\n";
                return false;
            }
            break;
        }

        current = next;
        moved = true;
    }

    if (!moved) {
        std::cout << who << ": at end.\n";
        return false;
    }

    if (!A.gotoRec(current) || !A.readCurrent()) {
        std::cout << who << ": failed to read record.\n";
        return false;
    }

    if (talk) std::cout << "Recno: " << A.recno() << "\n";
    return true;
}

} // namespace cli::nav