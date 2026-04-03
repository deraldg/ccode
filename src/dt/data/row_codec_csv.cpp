// src/dt/data/row_codec_csv.cpp
//
// Minimal RowSet ? CSV writer used by EXPORT.
// Focused on producing a reasonable CSV stream; import routines
// are stubs for now and can be fleshed out when IMPORT/DB_Converter
// need them.

#include "dt/data/row_codec_csv.hpp"

#include <ostream>
#include <istream>
#include <string>
#include <vector>

namespace dt::data {

Schema build_schema_from_csv_header(const std::vector<std::string>& header_fields) {
    Schema schema;
    schema.columns.reserve(header_fields.size());
    for (const auto& name : header_fields) {
        ColumnMeta c;
        c.name     = name;
        c.type     = CellType::Character; // could be inferred later
        c.width    = 0;
        c.decimals = 0;
        schema.columns.push_back(c);
    }
    return schema;
}

static void csv_escape_field(const std::string& in,
                             const CsvDialect&  dialect,
                             std::string&       out) {
    const char d = dialect.delimiter;
    const char q = dialect.quote_char;

    bool need_quotes = dialect.always_quote;
    out.clear();

    for (char ch : in) {
        if (ch == d || ch == q || ch == '\n' || ch == '\r') {
            need_quotes = true;
        }
    }

    if (!need_quotes) {
        out = in;
        return;
    }

    out.push_back(q);
    for (char ch : in) {
        if (ch == q) {
            out.push_back(q); // double quote to escape
        }
        out.push_back(ch);
    }
    out.push_back(q);
}

bool row_to_csv_line(const Row&          row,
                     const Schema&       schema,
                     const CsvDialect&   dialect,
                     std::string&        out_line,
                     std::string*        error_out) {
    (void)error_out; // not used yet

    const std::size_t cols = schema.columns.size();
    std::string field_buf;
    out_line.clear();

    for (std::size_t i = 0; i < cols; ++i) {
        if (i > 0) {
            out_line.push_back(dialect.delimiter);
        }

        std::string text;

        if (i < row.cells.size()) {
            const Cell& cell = row.cells[i];

            if (!cell.raw.empty()) {
                text = cell.raw;
            } else {
                if (auto s = std::get_if<std::string>(&cell.value)) {
                    text = *s;
                } else if (auto d = std::get_if<double>(&cell.value)) {
                    text = std::to_string(*d);
                } else if (auto b = std::get_if<bool>(&cell.value)) {
                    text = *b ? "T" : "F";
                } else if (auto date = std::get_if<DateYMD>(&cell.value)) {
                    if (date->is_valid()) {
                        // crude YYYY-MM-DD
                        text = std::to_string(date->year) + "-" +
                               std::to_string(date->month) + "-" +
                               std::to_string(date->day);
                    }
                }
            }
        }

        csv_escape_field(text, dialect, field_buf);
        out_line.append(field_buf);
    }

    return true;
}

bool csv_fields_to_row(const std::vector<std::string>& raw_fields,
                       const Schema&                   schema,
                       Row&                            out_row,
                       CsvImportMode                   /*mode*/,
                       const CsvDialect&               /*dialect*/,
                       std::string*                    error_out) {
    (void)error_out; // not used yet

    out_row.cells.clear();
    out_row.cells.resize(schema.columns.size());

    const std::size_t n = schema.columns.size();
    for (std::size_t i = 0; i < n; ++i) {
        Cell&       cell = out_row.cells[i];
        const auto& meta = schema.columns[i];

        cell.field_name  = meta.name;
        cell.field_index = static_cast<int>(i + 1);
        cell.type        = meta.type;
        cell.width       = meta.width;
        cell.decimals    = meta.decimals;

        if (i < raw_fields.size()) {
            cell.raw       = raw_fields[i];
            cell.value     = cell.raw;
            cell.has_value = true;
            cell.valid     = true;
        } else {
            cell.raw.clear();
            cell.value     = std::string{};
            cell.has_value = false;
            cell.valid     = true;
        }
    }

    return true;
}

bool write_rowset_as_csv(std::ostream&     os,
                         const RowSet&     rowset,
                         const CsvDialect& dialect,
                         std::string*      error_out) {
    (void)error_out; // not used yet

    const auto& schema = rowset.schema;
    std::string field_buf;
    std::string line;

    // Header
    if (dialect.has_header_row) {
        const std::size_t cols = schema.columns.size();
        for (std::size_t i = 0; i < cols; ++i) {
            if (i > 0) {
                os.put(dialect.delimiter);
            }
            const std::string& name = schema.columns[i].name;
            csv_escape_field(name, dialect, field_buf);
            os << field_buf;
        }
        os << "\n";
    }

    // Rows
    for (const Row& row : rowset.rows) {
        if (!row_to_csv_line(row, schema, dialect, line, error_out)) {
            return false;
        }
        os << line << "\n";
    }

    return true;
}

RowSet read_csv_to_rowset(std::istream&     is,
                          const Schema&     schema_hint,
                          const CsvDialect& dialect,
                          CsvImportMode     /*mode*/,
                          std::string*      error_out) {
    (void)dialect;
    (void)error_out; // both unused in this minimal stub

    RowSet rs;
    Schema schema = schema_hint;

    std::string line;
    bool first = true;

    while (std::getline(is, line)) {
        if (first && dialect.has_header_row && schema.columns.empty()) {
            // TODO: split header line and call build_schema_from_csv_header().
            first = false;
            continue;
        }
        first = false;

        Row row;
        row.origin.source = RowSource::Csv;

        // TODO: proper CSV split; for now, treat entire line as one field.
        std::vector<std::string> raw_fields;
        raw_fields.push_back(line);

        csv_fields_to_row(raw_fields, schema, row,
                          CsvImportMode::Strict, dialect, nullptr);
        rs.rows.push_back(std::move(row));
    }

    rs.schema = schema;
    return rs;
}

} // namespace dt::data



