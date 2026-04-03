// src/cli/cmd_use.cpp
// DotTalk++ USE command (open DBF in a work area) — duplicate-open guard, NOINDEX, auto-attach

#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <type_traits>

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/order_state.hpp"
#include "cli/order_hooks.hpp"      // to run reconcile_after_mutation()
#include "cli/cmd_setpath.hpp"
#include "cli/path_resolver.hpp"
#include "memo/memo_auto.hpp"

#include "cnx/cnx.hpp"              // reporting helper (CNX is deprecated but still supported)

using namespace xbase;
namespace fs = std::filesystem;

// --- engine access (why: scan all areas to prevent duplicate opens) ---
extern "C" xbase::XBaseEngine* shell_engine();

namespace {

// ----------------------- path & env helpers ---------------------------------

static fs::path find_data_root_guess()
{
    fs::path p = fs::current_path();
    for (int i = 0; i < 14; ++i) {
        fs::path cand = p / "data";
        if (fs::exists(cand) && fs::is_directory(cand)) {
            return fs::absolute(cand);
        }
        if (!p.has_parent_path()) break;
        fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return fs::absolute(fs::current_path());
}

static void ensure_setpath_initialized()
{
    using dottalk::paths::state;
    using dottalk::paths::init_defaults;
    using dottalk::paths::Slot;
    using dottalk::paths::get_slot;

    if (state().data_root.empty()) {
        init_defaults(find_data_root_guess());
        return;
    }
    if (get_slot(Slot::DBF).empty() || get_slot(Slot::INDEXES).empty()) {
        init_defaults(state().data_root);
    }
}

static bool looks_explicit_path(const std::string& s)
{
    if (s.find('/')  != std::string::npos) return true;
    if (s.find('\\') != std::string::npos) return true;
    if (s.size() >= 2 && std::isalpha((unsigned char)s[0]) && s[1] == ':') return true;
    if (!s.empty() && s[0] == '.') return true;
    return false;
}

static std::string strip_dbf_ext_if_present(std::string s)
{
    auto up = [](unsigned char c){ return (char)std::toupper(c); };
    if (s.size() >= 4) {
        const char a = up((unsigned char)s[s.size()-4]);
        const char b = up((unsigned char)s[s.size()-3]);
        const char c = up((unsigned char)s[s.size()-2]);
        const char d = up((unsigned char)s[s.size()-1]);
        if (a=='.' && b=='D' && c=='B' && d=='F') {
            s.resize(s.size()-4);
        }
    }
    return s;
}

static std::string up_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static bool contains_noindex(std::istringstream& iss)
{
    std::streampos pos = iss.tellg();
    if (pos == std::streampos(-1)) {
        return false;
    }

    bool found = false;
    std::string tok;
    while (iss >> tok) {
        const std::string u = up_copy(tok);
        if (u == "NOINDEX" || u == "NOIDX") {
            found = true;
            break;
        }
    }

    iss.clear();
    iss.seekg(pos);
    return found;
}

static void clear_order_best_effort(DbArea& a)
{
    // why: ensure physical order if requested
    try {
        orderstate::clearOrder(a);
        return;
    } catch (...) {}

    try {
        orderstate::setOrder(a, std::string{});
        return;
    } catch (...) {}
}

// ----------------------- SFINAE setters (optional APIs) ---------------------

template <typename T>
using has_setFilename_t = decltype(std::declval<T&>().setFilename(std::declval<std::string>()));
template <typename T, typename = has_setFilename_t<T>>
static inline void _setFilename(T& a, const std::string& s, int) { a.setFilename(s); }
template <typename T>
static inline void _setFilename(T&, const std::string&, long) {}

template <typename T>
using has_setLogicalName_t = decltype(std::declval<T&>().setLogicalName(std::declval<std::string>()));
template <typename T, typename = has_setLogicalName_t<T>>
static inline void _setLogicalName(T& a, const std::string& s, int) { a.setLogicalName(s); }
template <typename T>
static inline void _setLogicalName(T&, const std::string&, long) {}

template <typename T>
using has_setName_t = decltype(std::declval<T&>().setName(std::declval<std::string>()));
template <typename T, typename = has_setName_t<T>>
static inline void _setLegacyName(T& a, const std::string& s, int) { a.setName(s); }
template <typename T>
static inline void _setLegacyName(T&, const std::string&, long) {}

// ----------------------- area/find helpers ----------------------------------

static inline std::string s8(const fs::path& p) {
#if defined(_WIN32)
    auto u = p.u8string(); return std::string(u.begin(), u.end());
#else
    return p.string();
#endif
}

static fs::path canonicalish(const fs::path& p) {
    try { return fs::weakly_canonical(p); }
    catch (...) { return fs::absolute(p); }
}

static std::string path_key(const fs::path& p) {
    std::string s = s8(canonicalish(p));
#if defined(_WIN32)
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
#endif
    return s;
}

static int area_slot_of(DbArea& a) {
    auto* eng = shell_engine(); if (!eng) return -1;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (&eng->area(i) == &a) return i;
    }
    return -1;
}

static int find_open_area_for_path(const fs::path& dbf_path) {
    auto* eng = shell_engine(); if (!eng) return -1;
    const std::string target = path_key(dbf_path);
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        try {
            DbArea& A = eng->area(i);
            std::string fn = A.filename();
            if (fn.empty()) continue;
            if (path_key(fn) == target) return i;
        } catch (...) { /* ignore bad slot */ }
    }
    return -1;
}

static void populate_dbarea_metadata(DbArea& a, const fs::path& dbf_path) {
    const std::string abs = fs::absolute(dbf_path).string();
    const std::string stem = dbf_path.stem().string();
    _setFilename(a, abs, 0);     // SCHEMAS uses filename() as truth
    _setLogicalName(a, stem, 0); // optional alias
    _setLegacyName(a, stem, 0);  // legacy alias
}

// ----------------------- CNX uniqueness (reporting only) --------------------

static constexpr uint32_t TAGF_UNIQUE = 0x0001; // adjust if your CNX uses a different bit

static bool cnx_tag_is_unique(const std::string& cnx_path, const std::string& tag_upper)
{
    if (cnx_path.empty() || tag_upper.empty()) return false;

    cnxfile::CNXHandle* h = nullptr;
    if (!cnxfile::open(cnx_path, h)) return false;

    std::vector<cnxfile::TagInfo> tags;
    const bool ok = cnxfile::read_tagdir(h, tags);
    cnxfile::close(h);

    if (!ok) return false;

    for (const auto& t : tags) {
        if (up_copy(t.name) == up_copy(tag_upper)) {
            return (t.flags & TAGF_UNIQUE) != 0;
        }
    }
    return false;
}

// ----------------------- flavor / valid-index helpers -----------------------

static const char* area_kind_token_local(const DbArea& a)
{
    switch (a.kind()) {
        case AreaKind::V32:  return "v32";
        case AreaKind::V64:  return "v64";
        case AreaKind::V128: return "v128";
        case AreaKind::Tup:  return "tup";
        case AreaKind::Unknown:
        default:             return "unknown";
    }
}

// Current policy helper.
// If policy changes later (for example LMDB-backed CDX also allowed for v32),
// change this function only.
static const char* valid_index_types_for(const DbArea& a)
{
    switch (a.kind()) {
        case AreaKind::V32:  return "INX, CNX";
        case AreaKind::V64:  return "CDX";
        case AreaKind::V128: return "CDX";
        case AreaKind::Tup:  return "TUP";
        case AreaKind::Unknown:
        default:             return "(unknown)";
    }
}

static std::string open_display_name(const DbArea& a, const fs::path& dbf_path)
{
    if (!a.logicalName().empty()) return a.logicalName();
    if (!a.dbfBasename().empty()) return a.dbfBasename();
    return dbf_path.stem().string();
}

} // namespace

// ----------------------- Command entry --------------------------------------

void cmd_USE(DbArea& a, std::istringstream& iss)
{
    std::string name;
    iss >> name;

    if (name.empty()) {
        std::cout << "USE: missing table name.\n";
        return;
    }

    const bool noindex = contains_noindex(iss);
    ensure_setpath_initialized();

    // Resolve DBF path
    fs::path dbf_path;
    if (looks_explicit_path(name)) {
        // Explicit path: let resolver anchor relative paths and add .dbf if missing.
        dbf_path = dottalk::paths::resolve_dbf(name);
    } else {
        // Logical: "students" or "students.dbf"
        std::string base = strip_dbf_ext_if_present(name);
        dbf_path = dottalk::paths::get_slot(dottalk::paths::Slot::DBF) / (base + ".dbf");
    }

    // Duplicate-open guard (no-op if already open anywhere)
    const int cur_slot = area_slot_of(a);
    const int dup_slot = find_open_area_for_path(dbf_path);
    if (dup_slot >= 0) {
        if (dup_slot == cur_slot) {
            std::cout << "USE: '" << dbf_path.filename().string()
                      << "' is already open in current area " << cur_slot << ".\n";
        } else {
            std::cout << "USE: '" << dbf_path.filename().string()
                      << "' is already open in area " << dup_slot
                      << ". Close it first (e.g., SCHEMAS CLOSE " << dup_slot << ").\n";
        }
        return; // no-op by design
    }

    // Open DBF
    try {
        a.open(dbf_path.string());
        populate_dbarea_metadata(a, dbf_path);
    } catch (const std::exception& ex) {
        std::cout << "Open failed: " << ex.what() << "\n";
        return;
    } catch (...) {
        std::cout << "Open failed.\n";
        return;
    }

    // Memo auto-attach (best-effort, never fatal)
    {
        bool hasMemoFields = false;
        for (const auto& f : a.fields()) {
            if (f.type == 'M' || f.type == 'm') {
                hasMemoFields = true;
                break;
            }
        }

        const std::string openedPath = a.filename().empty()
            ? fs::absolute(dbf_path).string()
            : a.filename();

        std::string memo_err;
        if (!cli_memo::memo_auto_on_use(a, openedPath, hasMemoFields, memo_err)) {
            std::cout << "USE: memo attach failed: " << memo_err << "\n";
        }
    }

    // Standardized open report
    std::cout << "Opened " << open_display_name(a, dbf_path)
              << " (" << area_kind_token_local(a) << ")"
              << " : Record count " << a.recCount() << "\n";
    std::cout << "Valid Index/Indices   : " << valid_index_types_for(a) << "\n";

    // NOINDEX → force physical order; stop
    if (noindex) {
        clear_order_best_effort(a);
        std::cout << "NOINDEX: auto-attach skipped (physical order).\n";
        return;
    }

    // Auto-attach order (best-effort, never fatal)
    // Policy (temporary, for dev convenience):
    //   - only auto-attach INX/IDX if present beside the DBF
    //   - do NOT auto-attach CNX (deprecated; explicit use only)
    const fs::path opened_abs = fs::absolute(dbf_path);
    const fs::path dbf_dir = opened_abs.parent_path();
    const std::string base = opened_abs.stem().string();

    const fs::path inx_same_dir = dbf_dir / (base + ".inx");
    const fs::path idx_same_dir = dbf_dir / (base + ".idx");

    auto try_set_order = [&](const fs::path& p) {
        try {
            orderstate::setOrder(a, p.string());

            // Best-effort: for any order type, allow hooks to reconcile internal state.
            orderhooks::reconcile_after_mutation(a);

            // Report, including CNX tag/unique status if applicable.
            // (With current policy, CNX won't be auto-attached here, but keep reporting intact.)
            if (orderstate::isCnx(a)) {
                const std::string tag = orderstate::activeTag(a);
                if (!tag.empty()) {
                    const bool uniq = cnx_tag_is_unique(orderstate::orderName(a), tag);
                    std::cout << "Auto-attached order: " << p.filename().string()
                              << " (tag: " << tag << (uniq ? " [UNIQUE]" : "") << ")\n";
                } else {
                    std::cout << "Auto-attached order: " << p.filename().string() << "\n";
                }
            } else {
                std::cout << "Auto-attached order: " << p.filename().string() << "\n";
            }
        } catch (...) {
            // best-effort
        }
    };

    if (fs::exists(inx_same_dir)) {
        try_set_order(inx_same_dir);
    } else if (fs::exists(idx_same_dir)) {
        try_set_order(idx_same_dir);
    }
}