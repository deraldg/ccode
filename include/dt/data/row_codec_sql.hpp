\
    #pragma once

    #include <cstddef>
    #include <cstdint>
    #include <string>
    #include <string_view>
    #include <vector>

    #include "dt/data/cell.hpp"
    #include "dt/data/row.hpp"
    #include "dt/data/schema.hpp"
    #include "dt/data/rowset.hpp"

    namespace dt::data {

        // Target backend influences minor dialect differences:
        // - identifier quoting
        // - LIMIT/OFFSET vs TOP syntax (for future use)
        // - date/time literal style, etc.
        enum class SqlBackend {
            Generic,    // ANSI-ish default
            SqlServer,
            SQLite,
            Postgres,
            MySql,
            Oracle
        };

        // Metadata for a single SQL result column.
        struct SqlColumnMeta {
            std::string name;         // column name as reported by DB
            std::string type_name;    // DB-specific type name, e.g. "VARCHAR", "INT"
            bool        nullable  { true };
            int         length    { 0 };   // display length / max chars
            int         precision { 0 };   // for numeric types
            int         scale     { 0 };   // for numeric types

            SqlColumnMeta() = default;
        };

        // Direction for parameters when building prepared statements.
        enum class SqlParamDirection {
            In,
            Out,
            InOut
        };

        // A simple parameter binding description. The caller is expected
        // to translate CellValue into the client library's native binding
        // primitives.
        struct SqlParam {
            std::string       name;        // ":p1", "@p1", or column name
            CellValue         value;       // underlying scalar value
            SqlParamDirection direction { SqlParamDirection::In };
            bool              is_null   { false };

            SqlParam() = default;
        };

        // Map SQL column metadata to a dt::data::Schema.
        Schema build_schema_from_sql_columns(const std::vector<SqlColumnMeta>& cols,
                                             SqlBackend                        backend);

        // Build a Row from a vector of raw SQL values given as strings.
        bool build_row_from_sql_strings(const std::vector<std::string>& raw_values,
                                        const Schema&                   schema,
                                        RowOrigin                       origin,
                                        Row&                            out_row,
                                        std::string*                    error_out = nullptr,
                                        SqlBackend                      backend   = SqlBackend::Generic);

        // Variant: build a Row directly from CellValue vector (typed values).
        bool build_row_from_sql_values(const std::vector<CellValue>& values,
                                       const Schema&                 schema,
                                       RowOrigin                     origin,
                                       Row&                          out_row,
                                       std::string*                  error_out = nullptr,
                                       SqlBackend                    backend   = SqlBackend::Generic);

        // Convert a Row into a vector of parameter bindings in column order.
        bool row_to_sql_params(const Row&             row,
                               const Schema&          schema,
                               std::vector<SqlParam>& out_params,
                               std::string*           error_out = nullptr,
                               SqlBackend             backend   = SqlBackend::Generic);

        // Build a full INSERT statement for a single Row:
        //   INSERT INTO <table_name> (col1, col2, ...) VALUES (...);
        bool build_sql_insert_statement(const std::string&   table_name,
                                        const Schema&        schema,
                                        const Row&           row,
                                        SqlBackend           backend,
                                        bool                 use_parameters,
                                        std::string&         out_sql,
                                        std::string*         error_out = nullptr);

        // Build an UPDATE statement:
        //   UPDATE <table_name>
        //      SET col1 = ?, col2 = ?, ...
        //    WHERE key1 = ? AND key2 = ? ...
        bool build_sql_update_statement(const std::string&         table_name,
                                        const Schema&              schema,
                                        const Row&                 row,
                                        const std::vector<std::string>& key_columns,
                                        SqlBackend                 backend,
                                        bool                       use_parameters,
                                        std::string&               out_sql,
                                        std::string*               error_out = nullptr);

        // Helper to map a full RowSet into a batch of INSERT statements.
        bool build_sql_insert_batch(const std::string&   table_name,
                                    const RowSet&        rowset,
                                    SqlBackend           backend,
                                    bool                 use_parameters,
                                    std::vector<std::string>& out_sql_statements,
                                    std::string*         error_out = nullptr);

    } // namespace dt::data



