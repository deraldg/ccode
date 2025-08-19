#include "xindex/dbarea_adapt.hpp"
#include "xindex/dbarea_config.hpp"
#include "xbase.hpp"
#include <cstdlib>
#include <string>

namespace xindex {

// ---- String getter ----
std::string db_get_string(const xbase::DbArea& a, int recno, const std::string& field) {
#ifdef XINDEX_DB_GET_STRING
    return XINDEX_DB_GET_STRING(a, recno, field);
#else
    // Fallback: no knowledge of API -> return empty
    (void)a; (void)recno; (void)field;
    return std::string();
#endif
}

// ---- Numeric getter ----
double db_get_double(const xbase::DbArea& a, int recno, const std::string& field) {
#ifdef XINDEX_DB_GET_DOUBLE
    return XINDEX_DB_GET_DOUBLE(a, recno, field);
#else
    // Fallback: try to parse the string value, else 0.0
    std::string s = db_get_string(a, recno, field);
    if (!s.empty()) {
        char* endp = nullptr;
        double d = std::strtod(s.c_str(), &endp);
        if (endp && *endp == '\0') return d;
    }
    return 0.0;
#endif
}

// ---- Record count ----
int db_record_count(const xbase::DbArea& a) {
#ifdef XINDEX_DB_RECORD_COUNT
    return static_cast<int>(XINDEX_DB_RECORD_COUNT(a));
#else
    (void)a; return 0;
#endif
}

// ---- Deleted flag ----
bool db_is_deleted(const xbase::DbArea& a, int recno) {
#ifdef XINDEX_DB_IS_DELETED
    return !!XINDEX_DB_IS_DELETED(a, recno);
#else
    (void)a; (void)recno; return false;
#endif
}

// ---- Go to record ----
void db_goto_rec(xbase::DbArea& a, int recno) {
#ifdef XINDEX_DB_GOTO_REC
    XINDEX_DB_GOTO_REC(a, recno);
#else
    (void)a; (void)recno; // no-op
#endif
}

// ---- Current DBF path (for .inx filename) ----
std::string db_current_dbf_path(const xbase::DbArea& a) {
#ifdef XINDEX_DB_CURRENT_DBF_PATH
    return XINDEX_DB_CURRENT_DBF_PATH(a);
#else
    (void)a; return std::string();
#endif
}

} // namespace xindex
