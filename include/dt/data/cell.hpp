\
    #pragma once

    #include <cstdint>
    #include <string>
    #include <variant>

    namespace dt::data {

        // Logical type for a single cell's value.
        enum class CellType {
            Character,
            Numeric,
            Date,
            Logical,
            Memo,
            Unknown
        };

        // Where the cell conceptually came from.
        enum class CellOrigin {
            Field,      // Direct DBF field
            Computed,   // Expression/computed value
            Temp,       // Temporary variable / scratch
            Parameter   // Command parameter / argument
        };

        // Simple YYYY-MM-DD representation for DBF-style dates.
        struct DateYMD {
            std::int32_t year  {0};
            std::int32_t month {0};  // 1..12
            std::int32_t day   {0};  // 1..31

            [[nodiscard]] bool is_valid() const noexcept {
                return (year != 0 &&
                        month >= 1 && month <= 12 &&
                        day   >= 1 && day   <= 31);
            }
        };

        // Underlying value type carried by a Cell.
        using CellValue = std::variant<
            std::monostate,   // no value / uninitialized
            std::string,      // character/memo or date-as-text if needed
            double,           // numeric
            DateYMD,          // date in structured form
            bool              // logical
        >;

        // Value object representing a single field instance.
        struct Cell {
            // Logical typing & origin
            CellType   type      { CellType::Unknown };
            CellOrigin origin    { CellOrigin::Field };

            // Identity (for DBF-backed cells)
            std::string field_name;        // e.g. "LNAME"
            int         field_index { -1 }; // 1-based index in DBF; -1 if not applicable

            // Raw and parsed value
            std::string raw;               // original text (DBF bytes, CSV text, etc.)
            CellValue   value;             // parsed/typed representation
            bool        has_value { false }; // distinct from empty raw

            // Formatting hints
            int         width       { 0 };     // DBF length or preferred display width
            int         decimals    { 0 };     // numeric decimals
            bool        pad_right   { true };  // padding direction for character data
            std::string format_mask;          // optional: date/numeric mask

            // State / validation
            bool        dirty       { false }; // changed since read
            bool        valid       { true };  // parse/validation flag
            std::string error;                 // human-readable reason if !valid

            Cell() = default;
        };

    } // namespace dt::data



