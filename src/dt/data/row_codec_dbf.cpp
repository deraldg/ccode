// src/dt/data/row_codec_dbf.cpp
//
// Minimal DBF ? Row/RowSet bridge for EXPORT.
// Uses xbase::DbArea in the simplest possible way, without
// getting fancy about types yet. Enough to satisfy the
// dt::data interface and let EXPORT compile and run.

#include "dt/data/row_codec_dbf.hpp"

#include <string>
#include <vector>
#include <stdexcept>

#include "xbase.hpp"  // DbArea, fields(), get(), recCount(), gotoRec(), readCurrent()

namespace dt::data {

Schema build_schema_from_dbf(const xbase::DbArea& area) {
    Schema schema;

    const auto& fields = area.fields();
    schema.columns.reserve(fields.size());

    for (const auto& f : fields) {
        ColumnMeta c;
        c.name     = f.name;              // we know this exists from existing code
        c.type     = CellType::Character; // placeholder; refine later when we wire types
        c.width    = 0;
        c.decimals = 0;
        schema.columns.push_back(c);
    }

    return schema;
}

Row read_row_from_dbf(const xbase::DbArea& area,
                      const Schema&        schema,
                      std::string*         error_out,
                      const DbfRowCodecOptions& /*opts*/) {
    Row row;
    row.origin.source = RowSource::Dbf;
    row.origin.area_id = -1;  // CLI can fill if it cares
    row.origin.recno   = 0;   // we?re not querying recno() here yet

    const std::size_t col_count = schema.columns.size();
    row.cells.resize(col_count);

    try {
        for (std::size_t i = 0; i < col_count; ++i) {
            Cell&       cell = row.cells[i];
            const auto& meta = schema.columns[i];

            cell.field_name  = meta.name;
            cell.field_index = static_cast<int>(i + 1);
            cell.type        = meta.type;
            cell.width       = meta.width;
            cell.decimals    = meta.decimals;

            // DbArea::get(int index) is already used elsewhere in CLI.
            std::string v = area.get(static_cast<int>(i + 1));
            cell.raw       = v;
            cell.value     = v;
            cell.has_value = true;
            cell.valid     = true;
            cell.dirty     = false;
        }

        if (error_out) {
            error_out->clear();
        }
    } catch (const std::exception& ex) {
        if (error_out) {
            *error_out = ex.what();
        }
        for (auto& c : row.cells) {
            c.valid = false;
            if (c.error.empty() && error_out && !error_out->empty()) {
                c.error = *error_out;
            }
        }
    }

    return row;
}

bool write_row_to_dbf(xbase::DbArea&       /*area*/,
                      const Row&           /*row*/,
                      const Schema&        /*schema*/,
                      std::string*         error_out,
                      const DbfRowCodecOptions& /*opts*/) {
    // Read-only stub for now. EXPORT never calls this.
    if (error_out) {
        *error_out = "write_row_to_dbf: not implemented (read-only stub).";
    }
    return false;
}

RowSet read_rows_from_dbf(xbase::DbArea&       area,
                          const Schema&        schema,
                          std::size_t          limit,
                          std::string*         error_out,
                          const DbfRowCodecOptions& opts) {
    RowSet rs;
    rs.schema = schema;

    const int recs = area.recCount();
    std::size_t added = 0;
    std::string last_error;

    for (int r = 1; r <= recs; ++r) {
        if (limit != 0 && added >= limit) {
            break;
        }

        if (!area.gotoRec(r) || !area.readCurrent()) {
            continue; // skip unreadable rows
        }

        std::string err;
        Row row = read_row_from_dbf(area, schema, &err, opts);
        if (!err.empty()) {
            last_error = err;
        }

        rs.rows.push_back(std::move(row));
        ++added;
    }

    if (error_out) {
        *error_out = last_error;
    }

    return rs;
}

} // namespace dt::data



