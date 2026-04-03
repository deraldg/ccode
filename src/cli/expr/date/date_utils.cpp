// src/cli/expr/date/date_utils.cpp
#include "date_utils.hpp"

#include <sstream>
#include <iomanip>
#include <cctype>   // std::isdigit

namespace dottalk {
namespace date {

ClockSnapshot now_local() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    ClockSnapshot cs;
    std::ostringstream d, ti, dt;

    d.imbue(std::locale::classic());
    d << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900)
      << std::setw(2) << (tm.tm_mon + 1)
      << std::setw(2) << tm.tm_mday;

    ti.imbue(std::locale::classic());
    ti << std::setfill('0') << std::setw(2) << tm.tm_hour
       << std::setw(2) << tm.tm_min
       << std::setw(2) << tm.tm_sec;

    cs.date8 = d.str();
    cs.time6 = ti.str();

    dt << cs.date8 << cs.time6;
    cs.datetime14 = dt.str();

    return cs;
}

bool is_valid_ymd(int y, int m, int d) {
    if (y < 1 || y > 9999) return false;
    if (m < 1 || m > 12) return false;

    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int dim = mdays[m - 1];
    if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) {
        dim = 29;
    }
    return d >= 1 && d <= dim;
}

Date8 parse_ctod(const std::string& s) {
    std::string t = textio::trim(s);
    if (t.empty()) return "";

    // Extract only digits (handles YYYY-MM-DD, YYYY/MM/DD, YYYYMMDD, etc.)
    std::string digits;
    for (char c : t) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digits += c;
        }
    }

    if (digits.size() < 8) return "";
    digits = digits.substr(0, 8);  // truncate if extra junk

    int y = 0, m = 0, dd = 0;
    try {
        y  = std::stoi(digits.substr(0, 4));
        m  = std::stoi(digits.substr(4, 2));
        dd = std::stoi(digits.substr(6, 2));
    } catch (...) {
        return "";
    }

    if (!is_valid_ymd(y, m, dd)) return "";

    return digits;
}

} // namespace date
} // namespace dottalk