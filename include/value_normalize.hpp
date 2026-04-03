// value_normalize.hpp

#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>

namespace util {

// trim helpers
inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char ch){ return !std::isspace(ch); }));
    return s;
}
inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
    return s;
}
inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

// remove paired quotes if present: 'x' or "x"
inline std::string unquote(std::string s) {
    if (s.size() >= 2) {
        char a = s.front(), b = s.back();
        if ((a == '\'' && b == '\'') || (a == '"' && b == '"')) {
            return s.substr(1, s.size()-2);
        }
    }
    return s;
}

// Numeric parsing helpers
inline bool parse_integer(std::string_view in, long long &out) {
    // allow commas/underscores; strip them
    std::string cleaned;
    cleaned.reserve(in.size());
    for (char c : in) if (c != ',' && c != '_') cleaned.push_back(c);

    const char* first = cleaned.data();
    const char* last  = first + cleaned.size();
    auto ec = std::from_chars(first, last, out).ec;
    return (ec == std::errc() && first != last);
}

inline bool parse_real(std::string_view in, long double &out) {
    // allow commas/underscores
    std::string cleaned;
    cleaned.reserve(in.size());
    for (char c : in) if (c != ',' && c != '_') cleaned.push_back(c);
    try {
        size_t idx = 0;
        out = std::stold(cleaned, &idx);
        return idx == cleaned.size();
    } catch (...) { return false; }
}

// Date normalization: accept YYYYMMDD, YYYY-MM-DD, MM/DD/YYYY, M/D/YY(YY), "Nov 5 2025"
inline std::optional<std::string> normalize_date(std::string s) {
    s = trim(unquote(std::move(s)));
    if (s.empty()) return std::nullopt;

    auto digits_only = [](const std::string& t){
        std::string d; d.reserve(t.size());
        for (char c: t) if (std::isdigit((unsigned char)c)) d.push_back(c);
        return d;
    };

    // Case 1: pure 8 digits -> assume YYYYMMDD
    if (std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); })) {
        if (s.size() == 8) return s;
        // try YYMMDD (6) -> assume 20YY
        if (s.size() == 6) return std::string("20") + s; // crude but practical for your dataset
        return std::nullopt;
    }

    // Case 2: common separators -> strip and interpret
    {
        std::string d = digits_only(s);
        if (d.size() == 8) {
            // heuristics: if original looked like YYYY-MM-DD or YYYY/MM/DD, keep as YYYYMMDD
            // if looked like MM/DD/YYYY, the digit order will still be MMDDYYYY; we need to detect that.
            // Simple rule: if original begins with 4 digits -> YYYYMMDD else MMDDYYYY -> reorder.
            bool starts_with_year = std::isdigit((unsigned char)s[0]) &&
                                    std::isdigit((unsigned char)s[1]) &&
                                    std::isdigit((unsigned char)s[2]) &&
                                    std::isdigit((unsigned char)s[3]) &&
                                    (s.size() > 4 && !std::isdigit((unsigned char)s[4]));
            if (starts_with_year) return d;               // YYYYMMDD
            // MMDDYYYY -> to YYYYMMDD
            return d.substr(4,4) + d.substr(0,2) + d.substr(2,2);
        }
    }

    // Case 3: simple month names (en-US)
    {
        std::string lc = s;
        std::transform(lc.begin(), lc.end(), lc.begin(), [](unsigned char c){ return std::tolower(c); });
        auto month_num = [&](std::string_view)->int{
            static const char* names[] = {
              "jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"
            };
            for (int i=0;i<12;++i) {
                if (lc.find(names[i]) != std::string::npos) return i+1;
            }
            return 0;
        };
        int m = month_num(lc);
        if (m) {
            // pull digits in order they appear; expect day then year somewhere
            std::string d = "";
            for (char c: lc) if (std::isdigit((unsigned char)c)) d.push_back(c);
            if (d.size() == 3 || d.size() == 4) {
                // e.g., "Nov 5 25" or "Nov 05 25" -> assume 20YY
                int day = std::stoi(d.substr(0, (int)d.size()-2));
                int yr  = std::stoi(d.substr((int)d.size()-2, 2));
                int year = 2000 + yr;
                char buf[9];
                std::snprintf(buf, sizeof(buf), "%04d%02d%02d", year, m, day);
                return std::string(buf);
            }
            if (d.size() == 6 || d.size() == 8) {
                // day then year (2 or 4)
                int day = std::stoi(d.substr(0, (int)d.size()-4));
                int year = std::stoi(d.substr((int)d.size()-4, 4));
                char buf[9];
                std::snprintf(buf, sizeof(buf), "%04d%02d%02d", year, m, day);
                return std::string(buf);
            }
        }
    }

    return std::nullopt;
}

// Logical normalization: accept t/f, true/false, y/n, 1/0
inline std::optional<std::string> normalize_logical(std::string s) {
    s = trim(unquote(std::move(s)));
    if (s.empty()) return std::nullopt;
    std::string lc = s; std::transform(lc.begin(), lc.end(), lc.begin(), [](unsigned char c){ return std::tolower(c); });
    if (lc=="t"||lc=="true"||lc=="y"||lc=="yes"||lc=="1") return std::string("T");
    if (lc=="f"||lc=="false"||lc=="n"||lc=="no"||lc=="0")  return std::string("F");
    return std::nullopt;
}

/**
 * Coerce user input to the canonical comparable string for a field.
 * @param ftype One of 'C','N','D','L','M'
 * @param flen  Field length (used for C/N padding if you want; we mostly trim)
 * @param fdec  Field decimals for N (0 for integer)
 * @return normalized string on success; std::nullopt on invalid input.
 */

inline std::optional<std::string>
normalize_for_compare(char ftype, int flen, int fdec, std::string raw) {
    raw = trim(unquote(std::move(raw)));
    flen = flen + 1;
    if (ftype=='C') {
        // For character fields: just trim; do not pad; comparisons are already case-insensitive in callers.
        return raw;
    }
    if (ftype=='N') {
        if (raw.empty()) return std::nullopt;
        if (fdec<=0) {
            long long v=0;
            if (!parse_integer(raw, v)) return std::nullopt;
            return std::to_string(v);
        } else {
            long double v=0;
            if (!parse_real(raw, v)) return std::nullopt;
            // format without trailing zeros beyond fdec, but at least keep a dot if fdec>0
            std::ostringstream oss;
            oss.setf(std::ios::fixed); oss.precision(fdec);
            oss << v;
            std::string s = oss.str();
            // trim trailing zeros then possible trailing dot, but keep up to fdec places if non-zero
            while (!s.empty() && s.back()=='0') s.pop_back();
            if (!s.empty() && s.back()=='.') s.pop_back();
            return s.empty()? std::string("0") : s;
        }
    }
    if (ftype=='D') {
        return normalize_date(raw);
    }
    if (ftype=='L') {
        return normalize_logical(raw);
    }
    if (ftype=='M') {
        return raw;
//      return normalize_memo(raw); 
    }

    // Unknown types: fall back to raw (better than blocking)
    return raw;
}

} // namespace util



