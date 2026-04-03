#pragma once
#include <string>

// Map to your DbArea methods now that they return real values.
#define XINDEX_DB_FILENAME(a)        ((a).filename())
#define XINDEX_DB_RECORD_LENGTH(a)   ((a).recordLength())

namespace xbase { class DbArea; }

namespace xindex {
    // Existing required adapters (you likely already have these):
    int         db_record_count(const xbase::DbArea&);
    int         db_recno       (const xbase::DbArea&);
    std::string db_get_string  (const xbase::DbArea&, int recno, const std::string& field);
    double      db_get_double  (const xbase::DbArea&, int recno, const std::string& field);
    bool        db_is_deleted  (const xbase::DbArea&, int recno);

    // NEW: always-on optional adapters (no macro guards):
    std::string db_filename    (const xbase::DbArea&);  // "" if unknown
    int         db_record_length(const xbase::DbArea&); // -1 if unknown
}

#pragma once
// Map to your DbArea methods now that they return real values.
#define XINDEX_DB_FILENAME(a)        ((a).filename())
#define XINDEX_DB_RECORD_LENGTH(a)   ((a).recordLength())



