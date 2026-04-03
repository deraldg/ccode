\
    #pragma once

    #include <cstddef>
    #include <string>

    #include "dt/data/cell.hpp"
    #include "dt/data/row.hpp"

    namespace dt::data {

        struct RowErrorSummary {
            bool        ok              { true };
            std::size_t invalid_cells   { 0 };
            std::string first_error;    // message from the first invalid cell, if any
        };

        // Validate a single Cell in-place.
        // Implementations may:
        //  - check type/value consistency
        //  - enforce width/decimals constraints
        //  - set cell.valid and cell.error as needed.
        //
        // Returns cell.valid after validation. Optionally writes a short
        // error description into error_out when !cell.valid.
        bool validate_cell(Cell& cell, std::string* error_out = nullptr);

        // Validate every Cell in a Row.
        // Implementations typically call validate_cell() for each Cell.
        //
        // Returns true if all cells are valid under the current rules.
        bool validate_row(Row& row, RowErrorSummary& summary);

        // Convenience: summarize errors without mutating the Row.
        RowErrorSummary summarize_row_errors(const Row& row);

    } // namespace dt::data



