// src/cli/cmd_export.cpp
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "textio.hpp"

// New core data/codec headers
#include "dt/data/row.hpp"
#include "dt/data/rowset.hpp"
#include "dt/data/schema.hpp"
#include "dt/data/row_codec_dbf.hpp"
#include "dt/data/row_codec_csv.hpp"

using namespace xbase;

namespace {

static bool ieq_to(const std::string& s) {
    if (s.size() != 2) return false;
    return (s[0] == 'T' || s[0] == 't') && (s[1] == 'O' || s[1] == 'o');
}

// Simple helper to report errors in a consistent way.
static void print_error(const std::string& msg) {
    if (!msg.empty()) {
        std::cout << "ERROR: " << msg << "\n";
    }
}

} // namespace

void cmd_EXPORT(DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) {
        std::cout << "No file open\n";
        return;
    }

    // Accept either:
    //   EXPORT <file>
    // or
    //   EXPORT <table> TO <file>
    std::string tok1;
    if (!(iss >> tok1)) {
        std::cout << "Usage: EXPORT <table> TO <file>\n";
        return;
    }

    std::string dest;
    std::string tok2;
    std::streampos savePos = iss.tellg();
    if (iss >> tok2) {
        if (ieq_to(tok2)) {
            // form: <table> TO <file>
            if (!(iss >> dest) || dest.empty()) {
                std::cout << "Usage: EXPORT <table> TO <file>\n";
                return;
            }
        } else {
            // fallback to single-arg form: first token is the file name
            dest = tok1;
            iss.seekg(savePos); // put back tok2 for safety (not used)
        }
    } else {
        dest = tok1;
    }

    if (!textio::ends_with_ci(dest, ".csv")) {
        dest += ".csv";
    }

    std::ofstream out(dest, std::ios::binary);
    if (!out) {
        std::cout << "Unable to open " << dest << " for write\n";
        return;
    }

    // --- New Row/Cell-based pipeline starts here -------------------------

    // 1) Build Schema from the current DbArea
    dt::data::Schema schema = dt::data::build_schema_from_dbf(a);

    // 2) Read all records into a RowSet (natural table order)
    dt::data::DbfRowCodecOptions dbf_opts;
    // You can tune these later if you want different behavior:
    // dbf_opts.parse_dates           = true;
    // dbf_opts.parse_logicals        = true;
    // dbf_opts.parse_numeric         = true;
    // dbf_opts.allow_truncate_character = false;
    // dbf_opts.allow_round_numeric      = true;

    std::string dbf_error;
    dt::data::RowSet rowset = dt::data::read_rows_from_dbf(
        a,
        schema,
        /*limit=*/0,         // 0 = no limit; export all records
        &dbf_error,
        dbf_opts
    );

    if (!dbf_error.empty()) {
        print_error(dbf_error);
        // We still proceed if some records were read, but you can change this
        // to a hard abort if desired.
    }

    // 3) Configure CSV dialect (comma-separated, header row)
    dt::data::CsvDialect dialect;
    dialect.delimiter       = ',';
    dialect.quote_char      = '"';
    dialect.escape_char     = '"';
    dialect.has_header_row  = true;
    dialect.trim_whitespace = false;
    dialect.always_quote    = false;

    // 4) Write RowSet as CSV to the already-open stream
    std::string csv_error;
    bool ok = dt::data::write_rowset_as_csv(out, rowset, dialect, &csv_error);
    if (!ok) {
        print_error(csv_error);
        // We fall through to the summary message, but you may choose
        // to return early if you want a stricter behavior.
    }

    // Summary: use the RowSet size so we accurately report how many
    // records were written, independent of underlying recCount()
    const std::size_t exported = rowset.rows.size();
    std::cout << "Exported " << exported << " records to " << dest << "\n";
}



