#pragma once
// Plan A: Strict DBF field codecs with predictable packing rules.
// No file I/O; pure formatting functions.
#include <cstdint> // Add this include
#include <string>
#include <string_view>
#include <optional>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <vector>
#include <limits>
#include <stdexcept>

namespace cli_planA {

inline std::string rpad_spaces(std::string s, int width) {
    if (width <= 0) return {};
    if ((int)s.size() > width) s.resize(width);
    if ((int)s.size() < width) s.append(width - (int)s.size(), ' ');
    return s;
}

inline std::string lpad_spaces(std::string s, int width) {
    if (width <= 0) return {};
    if ((int)s.size() > width) s = s.substr((int)s.size() - width);
    if ((int)s.size() < width) s.insert(0, width - (int)s.size(), ' ');
    return s;
}

// ---- Character (C) ----
inline std::string pack_char(std::string_view in, int length) {
    std::string s(in);
    // Strip any embedded NULs (DBF expects spaces, not NUL padding)
    s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
    return rpad_spaces(std::move(s), length);
}

// ---- Numeric / Float (N/F) ----
// Format with fixed precision; total width must fit length (including sign and dot).
inline std::string pack_numeric(double value, int length, int dec, bool& ok) {
    ok = true;
    if (length <= 0) { ok = false; return {}; }
    if (dec < 0) dec = 0;
    std::ostringstream oss;
    oss.setf(std::ios::fixed); 
    oss.precision(dec);
    oss << value;
    std::string s = oss.str();

    // If too wide, attempt rounding to fit by reducing precision (as a last resort).
    if ((int)s.size() > length) {
        for (int p = std::max(0, dec - 1); p >= 0; --p) {
            std::ostringstream tryoss;
            tryoss.setf(std::ios::fixed);
            tryoss.precision(p);
            tryoss << value;
            s = tryoss.str();
            if ((int)s.size() <= length) break;
        }
    }
    if ((int)s.size() > length) {
        ok = false; // cannot fit
        return std::string(length, '*'); // visual overflow marker
    }
    return lpad_spaces(std::move(s), length);
}

// Accept numeric from string input
inline std::string pack_numeric_from_string(std::string_view in, int length, int dec, bool& ok) {
    try {
        // Allow leading/trailing spaces and commas/underscores
        std::string s(in);
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return c == ',' || c == '_'; }), s.end());
        double v = std::stod(s);
        return pack_numeric(v, length, dec, ok);
    } catch (...) {
        ok = false;
        return lpad_spaces("0", length);
    }
}

// ---- Date (D): YYYYMMDD ----
inline bool is_digits(std::string_view s) {
    for (char c : s) if (!std::isdigit((unsigned char)c)) return false;
    return true;
}

inline std::string pack_date(std::string_view in, bool& ok) {
    ok = true;
    std::string s(in);
    // Accept YYYY-MM-DD or YYYY/MM/DD; normalize to YYYYMMDD
    if (s.size() == 10 && (s[4] == '-' || s[4] == '/') && (s[7] == '-' || s[7] == '/')) {
        s.erase(7,1);
        s.erase(4,1);
    }
    if (s.size() == 0) return "00000000";
    if (s.size() != 8 || !is_digits(s)) {
        ok = false;
        return "00000000";
    }
    return s;
}

// ---- Logical (L): 'T','F','?' ----
inline char pack_logical_char(char c) {
    if (c == 0) return '?';
    c = (char)std::toupper((unsigned char)c);
    if (c=='Y') c='T';
    if (c=='N') c='F';
    if (c!='T' && c!='F' && c!='?') c='?';
    return c;
}

inline char pack_logical_from_string(std::string_view in) {
    if (in.empty()) return '?';
    return pack_logical_char(in.front());
}

// ---- Memo pointer (M): default 4-byte little-endian dBase pointer ----
// Some variants use 10-byte ASCII. We provide both.
inline std::string pack_memo_ptr_le32(uint32_t block_id, int length) {
    if (length < 4) return std::string(length, '\0');
    std::string out(length, '\0');
    out[0] = (char)(block_id & 0xFF);
    out[1] = (char)((block_id >> 8) & 0xFF);
    out[2] = (char)((block_id >> 16) & 0xFF);
    out[3] = (char)((block_id >> 24) & 0xFF);
    // Remaining bytes (if any) left as 0
    return out;
}

inline std::string pack_memo_ptr_ascii(uint32_t block_id, int length) {
    std::string s = std::to_string(block_id);
    return rpad_spaces(std::move(s), length);
}

} // namespace cli_planA



