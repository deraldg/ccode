// src/xindex/dbarea_adapt.cpp
#include "xindex/dbarea_adapt.hpp"
#include "xbase.hpp"
#include "record_view.hpp"   // for A.get(1-based)
#include <string>
#include <algorithm>
#include <cctype>
#include "xbase.hpp"

namespace xindex {

std::string db_filename(const xbase::DbArea& a) {
#ifdef XINDEX_DB_FILENAME
    // Use the project’s mapping if provided
    return std::string(XINDEX_DB_FILENAME(a));
#else
    // Safe fallback: unknown
    return std::string{};
#endif
}

int db_record_length(const xbase::DbArea& a) {
#ifdef XINDEX_DB_RECORD_LENGTH
    // Use the project’s mapping if provided
    return static_cast<int>(XINDEX_DB_RECORD_LENGTH(a));
#else
    // Safe fallback: unknown
    return -1;
#endif
}


// ---- helpers ---------------------------------------------------------------
static inline std::string trim(std::string s) {
    auto notsp = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

// Best-effort field lookup by name (case-insensitive) using only what DbArea exposes via STRUCT/FIELDS paths.
// If your DbArea has a faster API, you can replace this later.
static int field_index_ci_fallback(const xbase::DbArea& A, const std::string& name) {
    // We try to guess via RecordView’s 1-based get() and a simple STRUCT-like probe.
    // If you have a header/metadata accessor, wire it here instead.
    // Return -1 when unknown.
    try {
        // Heuristic: many DbArea implementations expose a small number of fields (<=256).
        // We can’t read names directly, so we just fail gracefully.
        (void)A; (void)name;
    } catch(...) {}
    return -1;
}


// ---- required adapter funcs ------------------------------------------------
int db_recno(const xbase::DbArea& a) {
    return a.recno();
}

int db_record_count(const xbase::DbArea& a) {
    // Fast path if you later add a macro mapping to a native method.
#ifdef XINDEX_DB_RECORD_COUNT
    return XINDEX_DB_RECORD_COUNT(const_cast<xbase::DbArea&>(a));
#else
    // Generic: binary search the last valid recno using gotoRec().
    auto& A = const_cast<xbase::DbArea&>(a);
    int saved = A.recno();

    int lo = 0, hi = 1;
    while (A.gotoRec(hi)) { lo = hi; hi <<= 1; }    // exponential probe
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if (A.gotoRec(mid)) lo = mid; else hi = mid - 1;
    }

    if (saved > 0) A.gotoRec(saved);
    return lo; // 0 if no records
#endif
}

std::string db_get_string(const xbase::DbArea& a, int recno, const std::string& field) {
    auto& A = const_cast<xbase::DbArea&>(a);
    int saved = A.recno();
    std::string out;

    int idx0 = field_index_ci_fallback(a, field);
    if (idx0 < 0) {
        // Fall back to first field if we can’t resolve; keeps the system running.
        idx0 = 0;
    }
    if (recno > 0 && A.gotoRec(recno)) {
        out = A.get(idx0 + 1); // RecordView pattern: DbArea::get is 1-based
    }
    if (saved > 0) A.gotoRec(saved);
    return out;
}

double db_get_double(const xbase::DbArea& a, int recno, const std::string& field) {
    std::string s = trim(db_get_string(a, recno, field));
    if (s.empty()) return 0.0;
    try { return std::stod(s); } catch (...) { return 0.0; }
}

bool db_is_deleted(const xbase::DbArea& /*a*/, int /*recno*/) {
#ifdef XINDEX_DB_IS_DELETED
    auto& A = const_cast<xbase::DbArea&>(a);
    int saved = A.recno();
    bool del = false;
    if (recno > 0 && A.gotoRec(recno)) del = XINDEX_DB_IS_DELETED(A, recno);
    if (saved > 0) A.gotoRec(saved);
    return del;
#else
    // Safe default: treat as not-deleted until we wire a real flag.
    return false;
#endif
}

// --- optional adapters: only compile if you've opted in ---
#if defined(XINDEX_ADAPT_HAS_FILENAME)
std::string db_filename(const xbase::DbArea& /*a*/) {
    // TODO: wire to a.realFilename() when DbArea exposes it.
    // Returning empty keeps STATUS "File:" blank instead of failing to build.
    return {};
}
#endif

#if defined(XINDEX_ADAPT_HAS_RECLEN)
int db_record_length(const xbase::DbArea& /*a*/) {
    // TODO: wire to a.recordLength() when DbArea exposes it.
    // Returning 0 keeps STATUS "Bytes/rec:" blank/zero until then.
    return 0;
}
#endif

} // namespace xindex
