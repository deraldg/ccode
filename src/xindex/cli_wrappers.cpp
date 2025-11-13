/* src/xindex/cli_wrappers.cpp
   Bridge wrappers using only public APIs + IndexManager.
   NO record_view usage; NO private DbArea members. */
#include <string>
#include <iostream>
#include <cctype>
#include <algorithm>
#include "xbase.hpp"
#include "xindex/attach.hpp"      // ensure_manager(DbArea&)
#include "xindex/index_manager.hpp"
#include "xindex/index_tag.hpp"

namespace xindex {

namespace {
    thread_local bool g_eof = true;                 // emulate old NEXT->EOF behavior
    thread_local std::string g_active_tag_scratch;  // for const char* returns

    static inline bool ieq_ascii(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned char ca = static_cast<unsigned char>(a[i]);
            unsigned char cb = static_cast<unsigned char>(b[i]);
            if (std::tolower(ca) != std::tolower(cb)) return false;
        }
        return true;
    }
    static inline int find_field_ci_public(const xbase::DbArea& area, const std::string& name) {
        const auto& fs = area.fields();
        for (size_t i = 0; i < fs.size(); ++i) {
            if (ieq_ascii(fs[i].name, name)) return static_cast<int>(i + 1); // 1-based
        }
        return 0;
    }
    static inline int first_char_field_public(const xbase::DbArea& area) {
        const auto& fs = area.fields();
        for (size_t i = 0; i < fs.size(); ++i) {
            if (fs[i].type == 'C' || fs[i].type == 'c') return static_cast<int>(i + 1);
        }
        return 0;
    }
    static inline bool ends_with_icase(std::string s, std::string suf) {
        if (s.size() < suf.size()) return false;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        std::transform(suf.begin(), suf.end(), suf.begin(), ::tolower);
        return s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    }
}

// ------- navigation / position ---------------------------------------------
bool db_move_top(xbase::DbArea& a)    { g_eof = false; return a.top(); }
bool db_move_next(xbase::DbArea& a)   { if (g_eof) return false; bool ok = a.skip(1); if (!ok) g_eof = true; return ok; }
bool db_move_bottom(xbase::DbArea& a) { g_eof = false; return a.bottom(); }
bool db_eof()                         { return g_eof; }
bool db_at_eof(const xbase::DbArea& /*a*/) { return g_eof; }

// ------- index/tag plumbing (real) -----------------------------------------
bool db_set_tag(xbase::DbArea& a, const std::string& tagName) { return ensure_manager(a).set_active(tagName); }
std::string db_get_tag(const xbase::DbArea& a) {
    auto& m = ensure_manager(const_cast<xbase::DbArea&>(a));
    auto  t = m.active();
    return t ? t->spec().tag : std::string();
}
bool        db_index_has_active(const xbase::DbArea& a) { return ensure_manager(const_cast<xbase::DbArea&>(a)).has_active(); }
int         db_index_tag_count(const xbase::DbArea& a)  { return static_cast<int>(ensure_manager(const_cast<xbase::DbArea&>(a)).listTags().size()); }
const char* db_index_active_tag(const xbase::DbArea& a) {
    auto& m = ensure_manager(const_cast<xbase::DbArea&>(a));
    auto  t = m.active();
    g_active_tag_scratch = t ? t->spec().tag : std::string();
    return g_active_tag_scratch.c_str();
}
bool db_index_clear(xbase::DbArea& a) { ensure_manager(a).clear_active(); return true; }
bool db_index_set_order(const std::string& tag, xbase::DbArea& a) { return ensure_manager(a).set_active(tag); }

// Direction: IndexManager has set_direction(bool) but no getter; report not-descending.
bool db_order_descending(const xbase::DbArea& /*a*/) { return false; }

// Load index sidecar or table-associated index.
bool db_index_load_file(const std::string& path, xbase::DbArea& a) {
    auto& m = ensure_manager(a);
    if (path.empty()) return m.load_for_table(a.filename());
    if (ends_with_icase(path, ".inx")) return m.load_json(path);
    return m.load_for_table(path);
}

// ------- seek (still conservative until we define key parsing) -------------
bool db_seek_key(const std::string& /*key*/, bool /*soft*/, xbase::DbArea& /*a*/) { return false; }

// ------- field helpers ------------------------------------------------------
int db_find_field_ci(const xbase::DbArea& a, const std::string& name) { return find_field_ci_public(a, name); }
int db_first_char_field(const xbase::DbArea& a)                        { return first_char_field_public(a); }

// NOTE: field getters are stubbed (no record_view usage in this target)
bool db_field_get_char(const std::string& /*name*/, std::string& out, const xbase::DbArea& /*a*/) { out.clear(); return false; }
bool db_field_get_numeric(const std::string& /*name*/, double& /*out*/, const xbase::DbArea& /*a*/) { return false; }
bool db_field_get_logical(const std::string& /*name*/, bool& /*out*/, const xbase::DbArea& /*a*/) { return false; }
bool db_field_get_date_yyyymmdd(const std::string& /*name*/, std::string& out, const xbase::DbArea& /*a*/) { out.clear(); return false; }

bool db_record_is_deleted(const xbase::DbArea& a) { return const_cast<xbase::DbArea&>(a).isDeleted(); }
bool db_delete_current(xbase::DbArea& a)          { return a.deleteCurrent(); }
bool db_position_valid(const xbase::DbArea& a) {
    auto& A = const_cast<xbase::DbArea&>(a);
    int rn = A.recno();
    return rn > 0 && rn <= A.recCount();
}
int  db_current_recno(const xbase::DbArea& a) { return const_cast<xbase::DbArea&>(a).recno(); }
std::string db_current_table_path(const xbase::DbArea& a) { return a.filename(); }

// ------- minimal renderer ---------------------------------------------------
void db_render_current(const xbase::DbArea& a) {
    auto& A = const_cast<xbase::DbArea&>(a);
    std::cout << "[" << A.recno() << "]\n";
}

} // namespace xindex
