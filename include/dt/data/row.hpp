\
    #pragma once

    #include <cstdint>
    #include <vector>

    #include "dt/data/cell.hpp"

    namespace dt::data {

        // Where this row conceptually came from.
        //
        // Storage backends:
        //   - Dbf  : from a DbArea record
        //   - Csv  : from a CSV row
        //   - Sql  : from an SQL result set row
        //
        // Derived / in-memory views:
        //   - Tuple: projection/join across areas
        //   - Temp : scratch / constructed in memory
        enum class RowSource {
            Dbf,
            Csv,
            Sql,
            Tuple,
            Temp
        };

        struct RowOrigin {
            RowSource   source    { RowSource::Temp };

            // For DBF-backed rows
            int         area_id   { -1 };     // work area index, if applicable
            std::int64_t recno    { 0 };      // record number, if applicable

            // For SQL-backed rows you can extend this later, e.g.:
            // std::string sql_cursor_id;
            // std::int64_t sql_row_index;
        };

        struct Row {
            RowOrigin          origin;
            std::vector<Cell>  cells;

            Row() = default;

            // Convenience helpers
            [[nodiscard]] std::size_t size() const noexcept { return cells.size(); }
            [[nodiscard]] bool empty() const noexcept { return cells.empty(); }
        };

    } // namespace dt::data



