#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "dt/data/format_kind.hpp"

namespace dt::data {

struct FormatProfile {
    FormatKind kind = FormatKind::Unknown;
    std::string profile_name;
    virtual ~FormatProfile() = default;
};

struct CsvProfile : public FormatProfile {
    char delimiter = ',';
    char quote_char = '"';
    char escape_char = '"';
    bool has_header_row = true;
    bool trim_whitespace = false;
    bool always_quote = false;

    CsvProfile() {
        kind = FormatKind::CSV;
    }
};

enum class FixedAlign {
    Left,
    Right
};

enum class FixedFieldKind {
    Text,
    Digits,
    DateYYYYMMDD,
    NumericFixed
};

struct FixedFieldSpec {
    std::string name;
    std::size_t width = 0;
    FixedAlign align = FixedAlign::Left;
    char fill = ' ';
    FixedFieldKind kind = FixedFieldKind::Text;
    int decimals = 0;
};

struct FixedProfile : public FormatProfile {
    std::vector<FixedFieldSpec> fields;
    std::string line_ending = "\r\n";

    FixedProfile() {
        kind = FormatKind::FIXED;
    }
};

} // namespace dt::data