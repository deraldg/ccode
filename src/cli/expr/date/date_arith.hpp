// src/expr/date/date_arith.hpp
#pragma once

#include <string>
#include "date_utils.hpp"  // for Date8 = std::string

namespace dottalk {
namespace date {

// Add/subtract days from a YYYYMMDD string
// Returns new YYYYMMDD or empty string on invalid input/overflow
Date8 add_days(const Date8& base_date8, int days);

// Compute difference in days: date2 - date1
// Returns days (positive if date2 > date1, negative if date2 < date1)
// Returns 0 on invalid input
int diff_days(const Date8& date1, const Date8& date2);

// Convenience: DATEADD equivalent (same as add_days)
inline Date8 date_add(const Date8& base, int days) {
    return add_days(base, days);
}

// Convenience: DATEDIFF equivalent
inline int date_diff(const Date8& d1, const Date8& d2) {
    return diff_days(d1, d2);
}

} // namespace date
} // namespace dottalk