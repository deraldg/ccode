// src/cli/expr/date/date_arith.cpp
#include "date_arith.hpp"

#include <cstdlib>   // std::atoi
#include <cstdio>    // std::snprintf

namespace dottalk {
namespace date {

// Helpers
static bool parse_date8(const Date8& d8, int& y, int& m, int& d) {
    if (d8.size() != 8) return false;
    y = std::atoi(d8.substr(0,4).c_str());
    m = std::atoi(d8.substr(4,2).c_str());
    d = std::atoi(d8.substr(6,2).c_str());
    return is_valid_ymd(y, m, d);
}

static Date8 make_date8(int y, int m, int d) {
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d", y, m, d);
    return std::string(buf);
}

// Moved UP so it's visible to add_days
static int days_in_month(int y, int m) {
    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 0;
    if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) {
        return 29;
    }
    return mdays[m - 1];
}

Date8 add_days(const Date8& base_date8, int days) {
    if (base_date8.empty() || base_date8.size() != 8) return "";

    int y = 0, m = 0, d = 0;
    if (!parse_date8(base_date8, y, m, d)) return "";

    int total_days = d + days;

    // Handle negative offsets (go back months/years)
    while (total_days <= 0) {
        --m;
        if (m < 1) {
            m = 12;
            --y;
        }
        total_days += days_in_month(y, m);
    }

    // Handle positive offsets (advance months/years)
    while (true) {
        int dim = days_in_month(y, m);
        if (total_days <= dim) {
            d = total_days;
            break;
        }
        total_days -= dim;
        ++m;
        if (m > 12) {
            m = 1;
            ++y;
        }
    }

    return make_date8(y, m, d);
}

int diff_days(const Date8& date1, const Date8& date2) {
    if (date1.empty() || date2.empty() || date1.size() != 8 || date2.size() != 8) {
        return 0;
    }

    int y1=0, m1=0, d1=0, y2=0, m2=0, d2=0;
    if (!parse_date8(date1, y1, m1, d1) || !parse_date8(date2, y2, m2, d2)) {
        return 0;
    }

    // Naive Julian Day Number approximation (accurate enough for 1900–2100 range)
    auto julian_day = [](int y, int m, int d) -> int {
        if (m <= 2) { y--; m += 12; }
        int a = y / 100;
        int b = a / 4;
        int c = 2 - a + b;
        return static_cast<int>(365.25 * (y + 4716)) +
               static_cast<int>(30.6001 * (m + 1)) + d + c - 1524;
    };

    return julian_day(y2, m2, d2) - julian_day(y1, m1, d1);
}

} // namespace date
} // namespace dottalk