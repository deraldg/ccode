// src/cli/expr/fn_date.cpp
// FoxPro-style date/time builtins — now using the centralized date_arith module

#include "cli/expr/fn_date.hpp"

#include "date/date_utils.hpp"
#include "date/date_arith.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace dottalk::expr {

namespace {

// --------------------------------------------------
// Builtin implementations (now thin wrappers)
// --------------------------------------------------

static std::string dt_date(const std::vector<std::string>& /*argv*/) {
    return dottalk::date::now_local().date8;
}

static std::string dt_today(const std::vector<std::string>& /*argv*/) {
    return dottalk::date::now_local().date8;
}

static std::string dt_time(const std::vector<std::string>& /*argv*/) {
    return dottalk::date::now_local().time6;
}

static std::string dt_now(const std::vector<std::string>& /*argv*/) {
    return dottalk::date::now_local().datetime14;
}

static std::string dt_datetime(const std::vector<std::string>& /*argv*/) {
    return dottalk::date::now_local().datetime14;
}

static std::string dt_ctod(const std::vector<std::string>& argv) {
    if (argv.empty()) return " ";
    auto d8 = dottalk::date::parse_ctod(argv[0]);
    return d8.empty() ? " " : d8;
}

static std::string dt_dtoc(const std::vector<std::string>& argv) {
    if (argv.empty()) return {};
    std::string s = dottalk::date::parse_ctod(argv[0]);   // normalizes input
    if (s.empty()) return {};

    // style support (0 = YYYYMMDD, 1 = YYYY-MM-DD, 2 = YYYY/MM/DD)
    int style = 0;
    if (argv.size() >= 2) {
        try {
            std::string st = argv[1];
            st.erase(std::remove_if(st.begin(), st.end(), ::isspace), st.end());
            if (!st.empty()) style = std::stoi(st);
        } catch (...) {}
    }

    if (style == 1) return s.substr(0,4) + "-" + s.substr(4,2) + "-" + s.substr(6,2);
    if (style == 2) return s.substr(0,4) + "/" + s.substr(4,2) + "/" + s.substr(6,2);
    return s;                     // default = YYYYMMDD
}

static std::string dt_dateadd(const std::vector<std::string>& argv) {
    if (argv.size() < 2) return " ";
    auto base = dottalk::date::parse_ctod(argv[0]);
    if (base.empty()) return " ";
    try {
        int delta = std::stoi(argv[1]);
        return dottalk::date::add_days(base, delta);
    } catch (...) {
        return " ";
    }
}

static std::string dt_datediff(const std::vector<std::string>& argv) {
    if (argv.size() < 2) return "0";
    auto d1 = dottalk::date::parse_ctod(argv[0]);
    auto d2 = dottalk::date::parse_ctod(argv[1]);
    if (d1.empty() || d2.empty()) return "0";
    int diff = dottalk::date::diff_days(d1, d2);
    return std::to_string(diff);
}

// --------------------------------------------------
// Function table (unchanged)
// --------------------------------------------------

static const BuiltinFnSpec kDateFns[] = {
    { "DATE",      0, 0, &dt_date },
    { "TODAY",     0, 0, &dt_today },
    { "TIME",      0, 0, &dt_time },
    { "NOW",       0, 0, &dt_now },
    { "DATETIME",  0, 0, &dt_datetime },
    { "CTOD",      1, 1, &dt_ctod },
    { "DTOC",      1, 2, &dt_dtoc },
    { "DATEADD",   2, 2, &dt_dateadd },
    { "DATEDIFF",  2, 2, &dt_datediff },
};

static constexpr std::size_t kCount = sizeof(kDateFns) / sizeof(kDateFns[0]);

} // namespace

const BuiltinFnSpec* date_fn_specs() {
    return kDateFns;
}

std::size_t date_fn_specs_count() {
    return kCount;
}

} // namespace dottalk::expr