// src/cli/cmd_normalize.cpp
//
// NORMALIZE command ? test harness for value_normalize.hpp + dt::predicate::Value
//
// Usage:
//   NORMALIZE <C|N|D|L> <len> [dec_if_N] <value...>
//
// Examples:
//   NORMALIZE C 20   "  Hello  "
//   NORMALIZE N 10 0 1,234
//   NORMALIZE N 10 2 1234.50
//   NORMALIZE D 8    11/05/2025
//   NORMALIZE L 1    yes
//
// This does not touch the current work area; DbArea is accepted only
// to match the CLI handler signature.

#include <sstream>
#include <string>
#include <iostream>
#include <cctype>

#include "xbase.hpp"

// existing normalize helper
#include "value_normalize.hpp"

// new predicate headers
#include "dt/predicate/predicate_engine.hpp"
#include "dt/predicate/value_convert.hpp"
#include "dt/predicate/loop_harness.hpp"

using namespace util;
namespace dp = dt::predicate;

static void normalize_usage() {
    std::cout
        << "Usage: NORMALIZE <C|N|D|L> <len> [dec_if_N] <value...>\n"
        << "  Example: NORMALIZE D 8 11/05/2025\n"
        << "           NORMALIZE N 10 2 1,234.50\n";
}

static void explain_character(const std::string& raw,
                              const std::string& normalized) {
    std::cout << "  type note : CHARACTER (C)\n";
    if (raw != normalized) {
        std::cout << "  effect    : trimmed/unquoted character value\n";
    } else {
        std::cout << "  effect    : value unchanged by normalization\n";
    }
}

static void explain_numeric(int fdec,
                            const std::string& normalized) {
    std::cout << "  type note : NUMERIC (N)\n";
    if (fdec <= 0) {
        std::cout << "  mode      : integer (fdec=0)\n";
    } else {
        std::cout << "  mode      : fixed-point with " << fdec
                  << " decimal place(s)\n";
    }
    std::cout << "  as number : " << normalized << "\n";
}

static void explain_date(const std::string& normalized) {
    std::cout << "  type note : DATE (D)\n";
    if (normalized.size() == 8 &&
        std::all_of(normalized.begin(), normalized.end(),
                    [](unsigned char c){ return std::isdigit(c); })) {
        int year  = std::stoi(normalized.substr(0, 4));
        int month = std::stoi(normalized.substr(4, 2));
        int day   = std::stoi(normalized.substr(6, 2));
        std::cout << "  calendar  : " << year << "-"
                  << (month < 10 ? "0" : "") << month << "-"
                  << (day   < 10 ? "0" : "") << day << "\n";
    } else {
        std::cout << "  calendar  : (non-standard date string)\n";
    }
}

static void explain_logical(const std::string& normalized) {
    std::cout << "  type note : LOGICAL (L)\n";
    if (normalized == "T") {
        std::cout << "  as bool   : TRUE\n";
    } else if (normalized == "F") {
        std::cout << "  as bool   : FALSE\n";
    } else {
        std::cout << "  as bool   : <unknown logical value>\n";
    }
}

static const char* value_type_name(dp::ValueType t) {
    switch (t) {
    case dp::ValueType::Null:      return "Null";
    case dp::ValueType::Character: return "Character";
    case dp::ValueType::Numeric:   return "Numeric";
    case dp::ValueType::Date:      return "Date";
    case dp::ValueType::Logical:   return "Logical";
    }
    return "Unknown";
}

static void print_value_view(const dp::Value& v) {
    std::cout << "  value.type: " << value_type_name(v.type) << "\n";
    std::cout << "  value.s   : [" << v.s << "]\n";
    std::cout << "  value.n   : " << v.n << "\n";
    std::cout << "  value.b   : " << (v.b ? "TRUE" : "FALSE") << "\n";
}

void cmd_NORMALIZE(xbase::DbArea& /*area*/, std::istringstream& iss) {
    char ftype = 0;
    if (!(iss >> ftype)) {
        normalize_usage();
        return;
    }

    int flen = 0;
    if (!(iss >> flen)) {
        normalize_usage();
        return;
    }

    // Optional decimals: ONLY for NUMERIC fields.
    int fdec = 0;
    std::streampos dec_pos = iss.tellg();
    char kind = static_cast<char>(std::toupper(
        static_cast<unsigned char>(ftype)));

    if (kind == 'N') {
        if (!(iss >> fdec)) {
            // No decimals provided; rewind and assume 0.
            fdec = 0;
            iss.clear();
            iss.seekg(dec_pos);
        }
    } else {
        // For C/D/L, do not attempt to parse decimals; leave fdec=0.
    }

    // The rest of the line is the raw value (possibly quoted / spaced).
    std::string rest;
    std::getline(iss, rest);

    // Trim leading spaces from the remainder
    if (!rest.empty()) {
        auto first = rest.find_first_not_of(" \t");
        if (first != std::string::npos) {
            rest.erase(0, first);
        } else {
            rest.clear();
        }
    }

    if (rest.empty()) {
        normalize_usage();
        return;
    }

    std::string raw = rest;
    auto normalized = normalize_for_compare(ftype, flen, fdec, std::move(rest));

    std::cout << "NORMALIZE: type=" << ftype
              << " len=" << flen
              << " dec=" << fdec << "\n";
    std::cout << "  raw       : [" << raw << "]\n";

    if (!normalized) {
        std::cout << "  normalized: <invalid input for this field type>\n";
        std::cout << "  note      : value_normalize.hpp rejected this input\n";
        return;
    }

    std::cout << "  normalized: [" << *normalized << "]\n";

    // Per-type explanation using only value_normalize.hpp rules.
    switch (kind) {
        case 'C':
            explain_character(raw, *normalized);
            break;
        case 'N':
            explain_numeric(fdec, *normalized);
            break;
        case 'D':
            explain_date(*normalized);
            break;
        case 'L':
            explain_logical(*normalized);
            break;
        default:
            std::cout << "  type note : UNKNOWN field type; "
                         "normalization fell back to raw string.\n";
            break;
    }

    // Now show the dt::predicate::Value view.
    std::string v_err;
    dp::Value v = dp::value_from_normalized(ftype, flen, fdec, *normalized, &v_err);

    if (!v_err.empty()) {
        std::cout << "  value_err : " << v_err << "\n";
    }
    print_value_view(v);
}



