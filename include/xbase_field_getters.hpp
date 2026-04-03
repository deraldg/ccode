#pragma once

#include <cctype>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "xbase.hpp"
#include "textio.hpp"

// Tiny namespace to avoid collisions with local helpers.
namespace xfg {

// -----------------------------------------------------------------------------
// Basic helpers
// -----------------------------------------------------------------------------
inline std::string rtrim_copy(std::string s) {
    while (!s.empty() &&
           std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

inline std::string ltrim_copy(std::string s) {
    std::size_t i = 0;
    while (i < s.size() &&
           std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    if (i) s.erase(0, i);
    return s;
}

inline std::string trim_copy(std::string s) {
    return rtrim_copy(ltrim_copy(std::move(s)));
}

inline char up_char(char c) noexcept {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

// -----------------------------------------------------------------------------
// Type helpers
//
// These helpers classify field kinds for retrieval/display purposes.
// They do not imply that full binary decoding is implemented yet.
// -----------------------------------------------------------------------------
inline bool is_textual_type(char t) noexcept {
    switch (up_char(t)) {
    case 'C':   // Character
    case 'V':   // VFP VarChar
        return true;
    case 'M':   // Memo field: DBF slot currently holds a token/reference
        return true;
    default:
        return false;
    }
}

inline bool is_numeric_ascii_type(char t) noexcept {
    switch (up_char(t)) {
    case 'N':   // Numeric (ASCII in row)
    case 'F':   // Float   (ASCII in row)
        return true;
    default:
        return false;
    }
}

inline bool is_binary_numeric_type(char t) noexcept {
    switch (up_char(t)) {
    case 'I':   // VFP Integer
    case 'Y':   // VFP Currency
    case 'B':   // VFP Double
        return true;
    default:
        return false;
    }
}

inline bool is_date_type(char t) noexcept {
    return up_char(t) == 'D';
}

inline bool is_datetime_type(char t) noexcept {
    return up_char(t) == 'T';
}

inline bool is_logical_type(char t) noexcept {
    return up_char(t) == 'L';
}

inline bool is_memo_type(char t) noexcept {
    return up_char(t) == 'M';
}

// -----------------------------------------------------------------------------
// Field resolution
// -----------------------------------------------------------------------------
inline int resolve_field_index_std(xbase::DbArea& db, const std::string& nameIn) {
    const std::string name = textio::trim(nameIn);
    const auto F = db.fields();
    for (int i = 0; i < static_cast<int>(F.size()); ++i) {
        if (textio::ieq(textio::trim(F[i].name), name)) return i;
    }
    return -1;
}

inline int resolve_field_index_std(const xbase::DbArea& db, const std::string& nameIn) {
    const std::string name = textio::trim(nameIn);
    const auto F = db.fields();
    for (int i = 0; i < static_cast<int>(F.size()); ++i) {
        if (textio::ieq(textio::trim(F[i].name), name)) return i;
    }
    return -1;
}

inline char getFieldType(xbase::DbArea& db, const std::string& name) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return '\0';
    const auto F = db.fields();
    return (idx0 < static_cast<int>(F.size())) ? F[idx0].type : '\0';
}

inline char getFieldType(const xbase::DbArea& db, const std::string& name) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return '\0';
    const auto F = db.fields();
    return (idx0 < static_cast<int>(F.size())) ? F[idx0].type : '\0';
}

// -----------------------------------------------------------------------------
// Raw field access
//
// These functions return DBF slot contents only.
// For M fields, that means the raw memo token/reference stored in the row,
// not the resolved memo payload.
// -----------------------------------------------------------------------------
inline std::string getFieldRawString(xbase::DbArea& db, const std::string& name) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return std::string{};
    return db.get(idx0 + 1); // API is 1-based
}

inline std::string getFieldAsString(xbase::DbArea& db, const std::string& name) {
    return rtrim_copy(getFieldRawString(db, name));
}

// -----------------------------------------------------------------------------
// Memo-aware resolved getter hook
//
// The callback receives the raw DBF slot contents for an M field
// (typically a token/reference), and may return resolved memo text.
// If no resolver is provided, or resolution does not return a value,
// the raw slot contents are returned unchanged.
//
// This keeps the header backend-neutral: actual DTX/FPT retrieval belongs
// in the supplied resolver, not in this header.
// -----------------------------------------------------------------------------
using MemoResolver = std::function<std::optional<std::string>(xbase::DbArea&, const std::string& rawToken)>;

inline std::string getFieldAsResolvedString(xbase::DbArea& db,
                                            const std::string& name,
                                            const MemoResolver& memoResolver = {}) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return std::string{};

    const auto F = db.fields();
    if (idx0 >= static_cast<int>(F.size())) return std::string{};

    const char t = up_char(F[idx0].type);
    std::string raw = rtrim_copy(db.get(idx0 + 1));

    if (t == 'M' && memoResolver) {
        if (auto resolved = memoResolver(db, raw)) {
            return *resolved;
        }
    }

    return raw;
}

// -----------------------------------------------------------------------------
// Numeric access
//
// Current practical stance:
//   - N/F are parsed from ASCII storage now.
//   - I/Y/B are recognized as VFP binary numeric families.
//   - Until DbArea::get() or lower-level row decoding becomes binary-aware for
//     those types, this helper still parses whatever string representation
//     DbArea::get() returns.
//
// This makes the helper safe to use now without overstating binary VFP support.
// -----------------------------------------------------------------------------
inline double parse_trimmed_double(std::string s) {
    s = trim_copy(std::move(s));
    if (s.empty()) throw std::runtime_error("empty numeric");
    std::size_t pos = 0;
    const double v = std::stod(s, &pos);
    if (pos != s.size()) throw std::runtime_error("trailing numeric data");
    return v;
}

inline double getFieldAsNumber(xbase::DbArea& db, const std::string& name) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) throw std::runtime_error("field not found");

    const auto F = db.fields();
    if (idx0 >= static_cast<int>(F.size())) throw std::runtime_error("field index out of range");

    const char t = up_char(F[idx0].type);
    const std::string raw = getFieldRawString(db, name);

    if (is_numeric_ascii_type(t) || is_binary_numeric_type(t)) {
        return parse_trimmed_double(raw);
    }

    throw std::runtime_error("field is not numeric");
}

inline std::optional<double> tryGetFieldAsNumber(xbase::DbArea& db, const std::string& name) {
    try {
        return getFieldAsNumber(db, name);
    } catch (...) {
        return std::nullopt;
    }
}

// -----------------------------------------------------------------------------
// Integer-oriented convenience
// -----------------------------------------------------------------------------
inline std::optional<std::int64_t> tryGetFieldAsInt64(xbase::DbArea& db, const std::string& name) {
    try {
        std::string s = trim_copy(getFieldRawString(db, name));
        if (s.empty()) return std::nullopt;

        std::size_t pos = 0;
        const long long v = std::stoll(s, &pos, 10);
        if (pos != s.size()) return std::nullopt;

        return static_cast<std::int64_t>(v);
    } catch (...) {
        return std::nullopt;
    }
}

// -----------------------------------------------------------------------------
// Logical access
// -----------------------------------------------------------------------------
inline std::optional<bool> tryGetFieldAsBool(xbase::DbArea& db, const std::string& name) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return std::nullopt;

    const auto F = db.fields();
    if (idx0 >= static_cast<int>(F.size())) return std::nullopt;
    if (!is_logical_type(F[idx0].type)) return std::nullopt;

    std::string s = trim_copy(getFieldRawString(db, name));
    if (s.empty()) return std::nullopt;

    const char c = up_char(s.front());
    if (c == 'T' || c == 'Y') return true;
    if (c == 'F' || c == 'N') return false;
    return std::nullopt;
}

// -----------------------------------------------------------------------------
// Date / datetime access
//
// These remain string-oriented for now.
// For 'T' fields, this returns the raw string representation currently exposed
// by DbArea::get(); it is not yet a full native VFP datetime decoder.
// -----------------------------------------------------------------------------
inline std::string getFieldAsDateString(xbase::DbArea& db, const std::string& name) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return std::string{};

    const auto F = db.fields();
    if (idx0 >= static_cast<int>(F.size())) return std::string{};

    const char t = up_char(F[idx0].type);
    if (!(t == 'D' || t == 'T')) return std::string{};

    return rtrim_copy(getFieldRawString(db, name));
}

// -----------------------------------------------------------------------------
// Generic display helper
//
// For memo fields:
//   - returns resolved memo text if a resolver is supplied and succeeds
//   - otherwise returns the raw token/reference stored in the DBF slot
//
// For non-memo fields:
//   - returns trimmed display text
// -----------------------------------------------------------------------------
inline std::string getFieldForDisplay(xbase::DbArea& db,
                                      const std::string& name,
                                      const MemoResolver& memoResolver = {}) {
    const int idx0 = resolve_field_index_std(db, name);
    if (idx0 < 0) return std::string{};

    const auto F = db.fields();
    if (idx0 >= static_cast<int>(F.size())) return std::string{};

    if (is_memo_type(F[idx0].type)) {
        return getFieldAsResolvedString(db, name, memoResolver);
    }

    return getFieldAsString(db, name);
}

} // namespace xfg