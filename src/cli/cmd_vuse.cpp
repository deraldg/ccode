// src/cli/cmd_vuse.cpp
// DotTalk++ VUSE command (sandbox USE clone)
// - duplicate-open guard
// - NOINDEX support
// - auto-attach DTX memo sidecar
// - auto-load same-root table INI companion into memory
// - keeps room for future VFP-specific behavior

#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <type_traits>
#include <fstream>
#include <vector>
#include <map>

#include "xbase.hpp"
#include "textio.hpp"
#include "cli/order_state.hpp"
#include "cli/order_hooks.hpp"
#include "cli/cmd_setpath.hpp"
#include "cli/path_resolver.hpp"
#include "memo/memo_auto.hpp"

#include "cnx/cnx.hpp"

using namespace xbase;
namespace fs = std::filesystem;

// engine access
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

static std::string low_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static std::string trim_copy(const std::string& s)
{
    return textio::trim(s);
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
    try {
        orderstate::clearOrder(a);
        return;
    } catch (...) {}

    try {
        orderstate::setOrder(a, std::string{});
        return;
    } catch (...) {}
}

static bool has_memo_fields(DbArea& a)
{
    try {
        const auto defs = a.fields();
        for (const auto& f : defs) {
            const char t = (char)std::toupper((unsigned char)f.type);
            if (t == 'M') return true;
        }
    } catch (...) {}
    return false;
}

// ----------------------- SFINAE setters -------------------------------------

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
        } catch (...) {}
    }
    return -1;
}

static void populate_dbarea_metadata(DbArea& a, const fs::path& dbf_path) {
    const std::string abs = fs::absolute(dbf_path).string();
    const std::string stem = dbf_path.stem().string();
    _setFilename(a, abs, 0);
    _setLogicalName(a, stem, 0);
    _setLegacyName(a, stem, 0);
}

// ----------------------- tiny INI loader/cache -------------------------------

struct IniEntry {
    std::string key;
    std::string value;
};

struct IniSection {
    std::string name;
    std::vector<IniEntry> entries;
};

struct TableIniCache {
    fs::path ini_path;
    std::vector<IniSection> sections;
};

static std::map<std::string, TableIniCache>& ini_cache_store()
{
    static std::map<std::string, TableIniCache> g;
    return g;
}

static std::string ltrim_copy(const std::string& s)
{
    std::size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    return s.substr(i);
}

static std::string rtrim_copy(const std::string& s)
{
    std::size_t i = s.size();
    while (i > 0 && std::isspace((unsigned char)s[i - 1])) --i;
    return s.substr(0, i);
}

static std::string trim_ini_copy(const std::string& s)
{
    return rtrim_copy(ltrim_copy(s));
}

static IniSection* find_section(std::vector<IniSection>& sections, const std::string& name)
{
    for (auto& s : sections) {
        if (low_copy(s.name) == low_copy(name)) return &s;
    }
    return nullptr;
}

static IniSection& ensure_section(std::vector<IniSection>& sections, const std::string& name)
{
    IniSection* found = find_section(sections, name);
    if (found) return *found;
    sections.push_back(IniSection{name, {}});
    return sections.back();
}

static bool load_ini_file(const fs::path& ini_path,
                          std::vector<IniSection>& out_sections,
                          std::string& err)
{
    out_sections.clear();
    err.clear();

    std::ifstream in(ini_path.string(), std::ios::in);
    if (!in) {
        err = "Could not open INI file";
        return false;
    }

    std::string line;
    std::string current_section;
    std::size_t line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::string t = trim_ini_copy(line);
        if (t.empty()) continue;
        if (t[0] == ';' || t[0] == '#') continue;

        if (t.front() == '[') {
            if (t.back() != ']') {
                err = "Malformed section header at line " + std::to_string(line_no);
                out_sections.clear();
                return false;
            }
            current_section = trim_ini_copy(t.substr(1, t.size() - 2));
            ensure_section(out_sections, current_section);
            continue;
        }

        const std::size_t eq = t.find('=');
        if (eq == std::string::npos) {
            err = "Missing '=' at line " + std::to_string(line_no);
            out_sections.clear();
            return false;
        }

        const std::string key = trim_ini_copy(t.substr(0, eq));
        const std::string val = trim_ini_copy(t.substr(eq + 1));
        if (key.empty()) {
            err = "Empty key at line " + std::to_string(line_no);
            out_sections.clear();
            return false;
        }

        IniSection& sec = ensure_section(out_sections, current_section);
        sec.entries.push_back(IniEntry{key, val});
    }

    return true;
}

static fs::path sibling_ini_path(const fs::path& dbf_path)
{
    fs::path p = dbf_path;
    p.replace_extension(".ini");
    return p;
}

static void try_auto_load_table_ini(const fs::path& dbf_path)
{
    const fs::path ini_path = sibling_ini_path(dbf_path);
    if (!fs::exists(ini_path)) {
        return;
    }

    std::vector<IniSection> sections;
    std::string err;
    if (!load_ini_file(ini_path, sections, err)) {
        std::cout << "VUSE: INI load failed: " << ini_path.filename().string()
                  << " (" << err << ")\n";
        return;
    }

    ini_cache_store()[path_key(dbf_path)] = TableIniCache{ini_path, sections};

    std::size_t entry_count = 0;
    for (const auto& s : sections) entry_count += s.entries.size();

    std::cout << "Auto-loaded INI: " << ini_path.filename().string()
              << " (" << sections.size() << " section"
              << (sections.size() == 1 ? "" : "s") << ", "
              << entry_count << " entr"
              << (entry_count == 1 ? "y" : "ies") << ")\n";
}

// ----------------------- CNX uniqueness (reporting only) --------------------

static constexpr uint32_t TAGF_UNIQUE = 0x0001;

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

static void try_auto_attach_order(DbArea& a, const fs::path& dbf_path)
{
    const fs::path opened_abs = fs::absolute(dbf_path);
    const fs::path dbf_dir = opened_abs.parent_path();
    const std::string base = opened_abs.stem().string();

    const fs::path inx_same_dir = dbf_dir / (base + ".inx");
    const fs::path idx_same_dir = dbf_dir / (base + ".idx");

    auto try_set_order = [&](const fs::path& p) {
        try {
            orderstate::setOrder(a, p.string());
            orderhooks::reconcile_after_mutation(a);

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
        } catch (...) {}
    };

    if (fs::exists(inx_same_dir)) {
        try_set_order(inx_same_dir);
    } else if (fs::exists(idx_same_dir)) {
        try_set_order(idx_same_dir);
    }
}

} // namespace

void cmd_VUSE(DbArea& a, std::istringstream& iss)
{
    std::string name;
    iss >> name;

    if (name.empty()) {
        std::cout << "VUSE: missing table name.\n";
        return;
    }

    const bool noindex = contains_noindex(iss);
    ensure_setpath_initialized();

    fs::path dbf_path;
    if (looks_explicit_path(name)) {
        dbf_path = dottalk::paths::resolve_dbf(name);
    } else {
        std::string base = strip_dbf_ext_if_present(name);
        dbf_path = dottalk::paths::get_slot(dottalk::paths::Slot::DBF) / (base + ".dbf");
    }

    const int cur_slot = area_slot_of(a);
    const int dup_slot = find_open_area_for_path(dbf_path);

    if (dup_slot >= 0) {
        if (dup_slot == cur_slot) {
            // Important sandbox behavior:
            // If the requested table is already open in the current area,
            // still attempt memo auto-attach, auto-INI load, and optional NOINDEX handling.
            populate_dbarea_metadata(a, dbf_path);

            {
                std::string memo_err;
                const bool hasMemo = has_memo_fields(a);
                if (!cli_memo::memo_auto_on_use(a, dbf_path.string(), hasMemo, memo_err)) {
                    std::cout << "VUSE: memo attach failed: " << memo_err << "\n";
                    return;
                }
            }

            try_auto_load_table_ini(dbf_path);

            std::cout << "VUSE: '" << dbf_path.filename().string()
                      << "' is already open in current area " << cur_slot
                      << " (memo/ini checked).\n";

            if (noindex) {
                clear_order_best_effort(a);
                std::cout << "NOINDEX: auto-attach skipped (physical order).\n";
                return;
            }

            try_auto_attach_order(a, dbf_path);
            return;
        } else {
            std::cout << "VUSE: '" << dbf_path.filename().string()
                      << "' is already open in area " << dup_slot
                      << ". Close it first.\n";
            return;
        }
    }

    try {
        a.open(dbf_path.string());
        populate_dbarea_metadata(a, dbf_path);
    } catch (const std::exception& ex) {
        std::cout << "Open failed: " << ex.what() << "\n";
        return;
    }

    // Sandbox memo attach: fail visibly if memo sidecar attach fails.
    {
        std::string memo_err;
        const bool hasMemo = has_memo_fields(a);
        if (!cli_memo::memo_auto_on_use(a, dbf_path.string(), hasMemo, memo_err)) {
            try { a.close(); } catch (...) {}
            std::cout << "VUSE: memo attach failed: " << memo_err << "\n";
            return;
        }
    }

    // Sandbox INI auto-load: best effort only.
    try_auto_load_table_ini(dbf_path);

    std::cout << "Opened " << fs::path(a.name()).filename().string()
              << " with " << a.recCount() << " records.\n";

    if (noindex) {
        clear_order_best_effort(a);
        std::cout << "NOINDEX: auto-attach skipped (physical order).\n";
        return;
    }

    try_auto_attach_order(a, dbf_path);
}