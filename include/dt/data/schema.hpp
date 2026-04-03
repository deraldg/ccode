\
    #pragma once

    #include <string>
    #include <vector>

    #include "dt/data/cell.hpp"

    namespace dt::data {

        struct ColumnMeta {
            std::string name;          // logical column/field name
            CellType    type  { CellType::Unknown };
            int         width { 0 };   // DBF length or preferred display width
            int         decimals { 0 }; // numeric decimals

            ColumnMeta() = default;
        };

        struct Schema {
            std::vector<ColumnMeta> columns;

            Schema() = default;

            [[nodiscard]] int index_of(const std::string& col_name) const {
                for (std::size_t i = 0; i < columns.size(); ++i) {
                    if (columns[i].name == col_name) {
                        return static_cast<int>(i);
                    }
                }
                return -1;
            }

            [[nodiscard]] std::size_t size() const noexcept {
                return columns.size();
            }
        };

    } // namespace dt::data



