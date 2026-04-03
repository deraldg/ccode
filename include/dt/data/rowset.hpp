\
    #pragma once

    #include <vector>

    #include "dt/data/row.hpp"
    #include "dt/data/schema.hpp"

    namespace dt::data {

        struct RowSet {
            Schema           schema;
            std::vector<Row> rows;

            RowSet() = default;

            [[nodiscard]] std::size_t rowCount() const noexcept {
                return rows.size();
            }

            [[nodiscard]] std::size_t columnCount() const noexcept {
                return schema.columns.size();
            }
        };

    } // namespace dt::data



