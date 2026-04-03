// src/cli/order_path_resolver.hpp
// DotTalk++ — Order/Path Resolver (CNX/INX/IDX) — header-only
// Drop-in utility for consistent path resolution and status strings.
//
// Usage (examples):
//   auto r = dottalk::order::resolve_path("students", data_root, dbf_dir);
//   if (r.exists && r.kind != IndexKind::NONE) { /* open r.abs_path */ }
//
//   auto aa = dottalk::order::auto_attach_for("students", data_root, dbf_dir);
//   if (aa.exists) { /* auto-attach aa */ }
//
//   std::string nice = dottalk::order::pretty_rel(data_root, r.abs_path);
//
// Notes:
//  * data_root: absolute or cwd for "data/" (your app already knows this).
//  * dbf_dir  : directory holding the open DBF (absolute or relative to data_root).
//  * We never rewrite a user-supplied explicit path; we resolve it as given.
//  * Basename probes prefer indexes/<base>.cnx, then .inx, then legacy dbf/<base>.idx,
//    then sibling of DBF (<dbf_dir>/<base>.cnx|.inx|.idx) as last resort.

#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <sys/stat.h>

namespace dottalk { namespace order {

enum class IndexKind { NONE=0, CNX, INX, IDX };

struct ResolvedIndex {
    IndexKind   kind { IndexKind::NONE };
    std::string base;      // e.g., "students"
    std::string abs_path;  // normalized absolute or cwd-based path we actually open
    std::string rel_path;  // pretty path relative to data_root for UI
    bool        exists { false };
};

// ---------- tiny fs helpers (portable enough for our use) ----------
inline bool file_exists(const std::string& p) {
    struct stat st{}; return ::stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFREG);
}
inline bool starts_with_drive_or_root(const std::string& s){
    if (s.empty()) return false;
#ifdef _WIN32
    if (s.size() >= 2 && std::isalpha((unsigned char)s[0]) && s[1]==':') return true;
#endif
    return s[0]=='/' || s[0]=='\\';
}
inline std::string slashify(std::string s){
    for (auto& c: s) if (c=='\\') c='/'; return s;
}
inline std::string join2(std::string a, const std::string& b){
    if (a.empty()) return slashify(b);
    a = slashify(a);
    if (!a.empty() && a.back()!='/') a.push_back('/');
    return a + slashify(b);
}
inline std::string dirname_of(const std::string& p){
    auto s = slashify(p);
    auto pos = s.find_last_of('/');
    return (pos==std::string::npos) ? std::string() : s.substr(0,pos);
}
inline std::string filename_of(const std::string& p){
    auto s = slashify(p);
    auto pos = s.find_last_of('/');
    return (pos==std::string::npos) ? s : s.substr(pos+1);
}
inline std::string stem_of(const std::string& p){
    auto f = filename_of(p);
    auto dot = f.find_last_of('.');
    return (dot==std::string::npos) ? f : f.substr(0,dot);
}
inline std::string ext_of(const std::string& p){
    auto f = filename_of(p);
    auto dot = f.find_last_of('.');
    if (dot==std::string::npos) return {};
    std::string e = f.substr(dot+1);
    for (auto& c: e) c = (char)std::tolower((unsigned char)c);
    return e;
}
// Make b relative to a if possible (best-effort).
inline std::string make_relative(std::string a, std::string b){
    a = slashify(a); b = slashify(b);
    if (!a.empty() && a.back()!='/') a.push_back('/');
    if (b.rfind(a,0)==0) return b.substr(a.size());
    return b;
}

// ---------- core resolution logic ----------
inline IndexKind kind_from_ext(const std::string& ext){
    if (ext=="cnx") return IndexKind::CNX;
    if (ext=="inx") return IndexKind::INX;
    if (ext=="idx") return IndexKind::IDX; // legacy single-order
    return IndexKind::NONE;
}

inline ResolvedIndex resolve_explicit(const std::string& input_path,
                                      const std::string& data_root){
    ResolvedIndex out;
    out.base    = stem_of(input_path);
    out.abs_path= slashify(input_path);   // resolve "as given" (relative to CWD or absolute)
    out.rel_path= make_relative(slashify(data_root), out.abs_path);
    out.exists  = file_exists(out.abs_path);
    out.kind    = kind_from_ext(ext_of(out.abs_path));
    return out;
}

inline ResolvedIndex probe_basename(const std::string& base,
                                    const std::string& data_root,
                                    const std::string& dbf_dir){
    // Probe in canonical order:
    // 1) indexes/<base>.cnx
    // 2) indexes/<base>.inx
    // 3) dbf/<base>.idx (legacy)
    // 4) <dbf_dir>/<base>.(cnx|inx|idx)
    const std::vector<std::pair<std::string,IndexKind>> probes = {
        { join2("indexes", base + ".cnx"), IndexKind::CNX },
        { join2("indexes", base + ".inx"), IndexKind::INX },
        { join2("dbf"    , base + ".idx"), IndexKind::IDX },
        { join2(dbf_dir  , base + ".cnx"), IndexKind::CNX },
        { join2(dbf_dir  , base + ".inx"), IndexKind::INX },
        { join2(dbf_dir  , base + ".idx"), IndexKind::IDX },
    };
    for (auto& pr : probes){
        auto abs = join2(data_root, pr.first);  // data_root-relative
        if (file_exists(abs)) {
            ResolvedIndex out;
            out.kind    = pr.second;
            out.base    = base;
            out.abs_path= abs;
            out.rel_path= pr.first;             // already relative to data_root
            out.exists  = true;
            return out;
        }
    }
    // Not found; return a NONEXISTENT result pointing to the "first" candidate (cnx)
    ResolvedIndex out;
    out.kind    = IndexKind::NONE;
    out.base    = base;
    out.abs_path= join2(data_root, join2("indexes", base + ".cnx"));
    out.rel_path= join2("indexes", base + ".cnx");
    out.exists  = false;
    return out;
}

// Public, unified entry point.
// input may be: explicit path w/ or w/o extension, or a bare basename.
inline ResolvedIndex resolve_path(const std::string& input,
                                  const std::string& data_root,
                                  const std::string& dbf_dir){
    const bool has_sep = (input.find('\\')!=std::string::npos) || (input.find('/')!=std::string::npos);
    const bool has_ext = !ext_of(input).empty();
    if (has_sep || has_ext) {
        return resolve_explicit(input, data_root);
    }
    return probe_basename(input, data_root, dbf_dir);
}

// Auto-attach probe used by USE <table>
inline ResolvedIndex auto_attach_for(const std::string& table_base,
                                     const std::string& data_root,
                                     const std::string& dbf_dir){
    return probe_basename(table_base, data_root, dbf_dir);
}

// Pretty path for UI banners; prefers relative-to-data.
inline std::string pretty_rel(const std::string& data_root,
                              const std::string& abs_path){
    return make_relative(slashify(data_root), slashify(abs_path));
}

// Optional textual description for AREA banner like:
//   "via CNX [indexes/students.cnx]"  or  "via INX [dbf/students.inx]"
inline std::string via_phrase(IndexKind k, const std::string& rel_path){
    switch (k){
        case IndexKind::CNX: return std::string("via CNX [") + rel_path + "]";
        case IndexKind::INX: return std::string("via INX [") + rel_path + "]";
        case IndexKind::IDX: return std::string("via IDX [") + rel_path + "]";
        default:             return {};
    }
}

}} // namespace dottalk::order
