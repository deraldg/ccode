// src/cli/expr/date/date_utils.hpp
#pragma once

#include <string>
#include <ctime>

#include "textio.hpp"  // textio::trim assumed to be available

namespace dottalk {
namespace date {

// Temporary: YYYYMMDD as string (matches current DBF / shell usage)
using Date8 = std::string;

struct ClockSnapshot {
    Date8 date8;           // "YYYYMMDD"
    std::string time6;     // "HHMMSS"
    std::string datetime14; // "YYYYMMDDHHMMSS"
};

// Get current local date/time as strings
ClockSnapshot now_local();

// Validate year-month-day (handles leap years)
bool is_valid_ymd(int year, int month, int day);

// Parse CTOD-style input → Date8 or empty string on failure
// Accepts: "YYYY-MM-DD", "YYYY/MM/DD", "YYYYMMDD", unquoted digits, etc.
Date8 parse_ctod(const std::string& input);

// Placeholder: later can apply SET DATE / SET CENTURY formatting
inline std::string format_date8(const Date8& d8) {
    return d8;
}

} // namespace date
} // namespace dottalk