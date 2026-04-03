\
    #pragma once

    #include <cstddef>
    #include <string>

    #include "dt/data/row.hpp"
    #include "dt/data/rowset.hpp"
    #include "dt/data/schema.hpp"

    // Forward declare xbase types to avoid heavy includes here.
    namespace xbase {
        class DbArea;
    }

    namespace dt::data {

        // How strict we want to be when writing Rows back into DBF records.
        enum class DbfWriteMode {
            Strict,      // any invalid cell aborts the write
            Warn,        // attempt write; report issues via error_out
            PassThrough  // prefer raw text; minimal validation
        };

        struct DbfRowCodecOptions {
            DbfWriteMode write_mode { DbfWriteMode::Strict };

            // How to interpret DBF contents when building Cells
            bool trim_character_fields { true };
            bool parse_dates           { true };   // YYYYMMDD ? DateYMD
            bool parse_logicals        { true };   // 'T'/'F' ? bool
            bool parse_numeric         { true };   // numeric fields ? double

            // When writing back, allow truncation or treat as error
            bool allow_truncate_character { false };
            bool allow_round_numeric      { true };
        };

        // Build a Schema from an open DbArea's field metadata.
        // Implementations typically call DbArea::fields() or equivalent.
        Schema build_schema_from_dbf(const xbase::DbArea& area);

        // Read the current record of a DbArea into a Row.
        // If schema.columns is empty, implementation may call build_schema_from_dbf().
        //
        // Returns a Row whose origin.source == RowSource::Dbf, and whose origin
        // area_id/recno are populated when possible.
        Row read_row_from_dbf(const xbase::DbArea& area,
                              const Schema&        schema,
                              std::string*         error_out = nullptr,
                              const DbfRowCodecOptions& opts = {});

        // Write a Row back into the current record of a DbArea.
        // The Row is expected to be compatible with the provided Schema.
        //
        // Returns true on success; false on failure (details may go into error_out).
        bool write_row_to_dbf(xbase::DbArea&       area,
                              const Row&           row,
                              const Schema&        schema,
                              std::string*         error_out = nullptr,
                              const DbfRowCodecOptions& opts = {});

        // Convenience: read multiple records (up to limit; 0 means "no limit")
        // from the current DbArea into a RowSet.
        //
        // Implementation may move the record pointer; callers should not assume
        // previous recno is preserved.
        RowSet read_rows_from_dbf(xbase::DbArea&       area,
                                  const Schema&        schema,
                                  std::size_t          limit      = 0,
                                  std::string*         error_out  = nullptr,
                                  const DbfRowCodecOptions& opts = {});

    } // namespace dt::data



