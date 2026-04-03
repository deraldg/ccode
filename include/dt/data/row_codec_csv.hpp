\
    #pragma once

    #include <cstddef>
    #include <string>
    #include <vector>
    #include <iosfwd>

    #include "dt/data/row.hpp"
    #include "dt/data/rowset.hpp"
    #include "dt/data/schema.hpp"

    namespace dt::data {

        // Dialect/settings for CSV parsing/formatting.
        struct CsvDialect {
            char delimiter        { ',' };
            char quote_char       { '"' };
            char escape_char      { '"' };
            bool has_header_row   { true };
            bool trim_whitespace  { true };
            bool always_quote     { false };  // if false, quote only when needed
        };

        // How strict an IMPORT should be with invalid cells/rows.
        enum class CsvImportMode {
            Strict,       // any invalid cell ? row rejected
            SkipBadRows,  // skip invalid rows; report via error_out/log
            Repair        // attempt to default invalid cells; keep row
        };

        // Build a Schema from a CSV header row (typically the first line).
        // header_fields are already parsed/decoded fields from the header line.
        Schema build_schema_from_csv_header(const std::vector<std::string>& header_fields);

        // Convert a Row into a single CSV-formatted line (without trailing newline).
        // The Schema defines the column order and types.
        //
        // Returns true on success; false on error (e.g. invalid cell under Strict mode).
        bool row_to_csv_line(const Row&          row,
                             const Schema&       schema,
                             const CsvDialect&   dialect,
                             std::string&        out_line,
                             std::string*        error_out = nullptr);

        // Parse a CSV record (already split into raw fields) into a Row.
        // Implementation maps raw_fields ? Cells according to Schema.
        //
        // Returns true if the row is considered acceptable under the given mode.
        // When false, caller may drop the row or inspect error_out.
        bool csv_fields_to_row(const std::vector<std::string>& raw_fields,
                               const Schema&                   schema,
                               Row&                            out_row,
                               CsvImportMode                   mode,
                               const CsvDialect&               dialect,
                               std::string*                    error_out = nullptr);

        // Write a full RowSet to an output stream as CSV.
        // If dialect.has_header_row is true, implementation should emit header.
        bool write_rowset_as_csv(std::ostream&     os,
                                 const RowSet&     rowset,
                                 const CsvDialect& dialect,
                                 std::string*      error_out = nullptr);

        // Read CSV from an input stream into a RowSet, using the specified Schema
        // (if non-empty). When schema.columns is empty and dialect.has_header_row
        // is true, implementation may infer Schema from the header row.
        RowSet read_csv_to_rowset(std::istream&     is,
                                  const Schema&     schema_hint,
                                  const CsvDialect& dialect,
                                  CsvImportMode     mode,
                                  std::string*      error_out = nullptr);

    } // namespace dt::data



