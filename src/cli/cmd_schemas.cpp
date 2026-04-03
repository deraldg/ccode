// src/cli/cmd_schemas.cpp
//
// SCHEMAS (legacy DBF areas)
//
// Commands:
//   SCHEMAS                                   : List open areas.
//   SCHEMAS OPEN [<dir>]                      : Open all .dbf in <dir> (non-recursive).
//   SCHEMAS OPEN <dir> recursive              : (STUB) Accepts 'recursive'; falls back to non-recursive.
//   SCHEMAS OPEN <file.dbf>                   : Open a single .dbf into the CURRENT area.
//
//   SCHEMAS OPEN <target> CNX [FALLBACK] [recursive] [TABLE]
//   SCHEMAS OPEN <target> INX [FALLBACK] [recursive] [TABLE]
//   SCHEMAS OPEN <target> CDX [FALLBACK] [recursive] [TABLE]
//
//   NOTE:
//   - If CNX/INX/CDX is NOT specified, SCHEMAS OPEN will NOT attach/open indexes.
//     (DBF sidecars like memo are handled by the DBF open path, not by SCHEMAS.)
//   - TABLE flag will TABLE-ON each opened workarea (open DBF areas only).
//
//   SCHEMAS CLOSE                             : Close all.
//   SCHEMAS CLOSE <n> [m ...]                 : Close by area index(es).
//   SCHEMAS CLOSE <name|file|stem|alias>[,...]: Close by name(s)/alias(es); case-insensitive.
//   SCHEMAS SAVE <file>                       : Save layout (+relations if available), including index type + active tag.
//   SCHEMAS LOAD <file>                       : Close all, load layout (+relations), resolve relative/cross-OS paths, and restore tags.
//
// Notes:
// - filename() is treated as "open" truth; we set it on open so LIST/CLOSE work uniformly.
// - Alias is optional; we only read/write/set it if DbArea exposes the API.
// - OPEN resolves indexes like LOAD: sibling first, then INDEXES slot.
// - CNX resolves .cnx first, then .cdx for compatibility.
// - Relations integration is optional and zero-cost when headers absent.
//
// IMPORTANT SYNTAX RULE:
// - The directory/target is always the first argument after OPEN.
//   Examples:
//     SCHEMAS OPEN dbf TABLE
//     SCHEMAS OPEN dbf CNX TABLE
//     SCHEMAS OPEN table TABLE
//     SCHEMAS OPEN DBF CNX TABLE
//
// PATH RULE:
// - Relative OPEN targets are resolved against the configured path slots,
//   primarily the DBF slot established by INIT / SETPATH.
// - Common shorthand such as `SCHEMAS OPEN dbf` and `SCHEMAS OPEN students`
//   are treated as DBF-slot-relative requests.
//
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "cli/dirty_prompt.hpp"
#include "cli/order_state.hpp"
#include "cli/path_resolver.hpp"
#include "cli/cmd_setpath.hpp"

#define HAVE_PATHS 1

#if __has_include("set_relations.hpp")
  #include "set_relations.hpp"
  #define HAVE_RELATIONS 1
#else
  #define HAVE_RELATIONS 0
#endif

#if __has_include("cli/table_state.hpp")
  #include "cli/table_state.hpp"
  #define HAVE_TABLE 1
#else
  #define HAVE_TABLE 0
#endif

namespace fs = std::filesystem;
using std::string;

// CNX (compound) extensions
static constexpr const char* kCnxPrimaryExt = ".cnx";
static constexpr const char* kCnxCompatExt  = ".cdx";

// --------- Utilities --------------------------------------------------------

static inline string trim_copy(string s) {
    auto is_space = [](unsigned char ch){ return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c){ return !is_space(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c){ return !is_space(c); }).base(), s.end());
    return s;
}

static inline string to_lower(string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

static inline string to_upper(string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

static inline bool ci_equal(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

static inline std::string s8(const fs::path& p) {
#if defined(_WIN32)
    auto u = p.u8string();
    return std::string(u.begin(), u.end());
#else
    return p.string();
#endif
}

static inline bool ieq_ext(const fs::path& p, const char* extDotLower) {
    std::string e = p.extension().string();
    const size_t nRef = std::char_traits<char>::length(extDotLower);
    if (e.size() != nRef) return false;
    for (size_t i = 0; i < nRef; ++i) {
        unsigned char A = static_cast<unsigned char>(e[i]);
        unsigned char B = static_cast<unsigned char>(extDotLower[i]);
        if (std::tolower(A) != std::tolower(B)) return false;
    }
    return true;
}

static inline bool is_dbf(const fs::directory_entry& de) {
    return de.is_regular_file() && ieq_ext(de.path(), ".dbf");
}

static inline bool try_parse_int(const string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0') return false;
    if (v < INT_MIN || v > INT_MAX) return false;
    out = static_cast<int>(v);
    return true;
}

static std::vector<string> split_tokens(const string& s) {
    std::vector<string> out;
    string cur;
    for (char c : s) {
        if (c == ',' || std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    for (auto& t : out) t = trim_copy(t);
    out.erase(std::remove_if(out.begin(), out.end(), [](const string& t){ return t.empty(); }), out.end());
    return out;
}

static inline bool parse_fallback_ci(const std::string& token) {
    return ci_equal(token, "fallback") || ci_equal(token, "--fallback");
}

static inline bool parse_recursive_ci(const std::string& token) {
    return ci_equal(token, "recursive") || ci_equal(token, "--recursive") || ci_equal(token, "-r");
}

static inline bool parse_table_ci(const std::string& token) {
    return ci_equal(token, "table") || ci_equal(token, "--table");
}

// Cross-OS path recognition / translation for LOAD.
static bool looks_like_windows_abs(const fs::path& p) {
    const std::string s = s8(p);
    return s.size() >= 3 &&
           std::isalpha(static_cast<unsigned char>(s[0])) &&
           s[1] == ':' &&
           (s[2] == '\\' || s[2] == '/');
}

static bool looks_like_posix_abs(const fs::path& p) {
    const std::string s = s8(p);
    return !s.empty() && s[0] == '/';
}

static fs::path translate_cross_os_absolute(const fs::path& p) {
    const std::string s = s8(p);

#if defined(_WIN32)
    // /mnt/x/... -> X:\...
    if (s.size() >= 7 &&
        s[0] == '/' && s[1] == 'm' && s[2] == 'n' && s[3] == 't' && s[4] == '/' &&
        std::isalpha(static_cast<unsigned char>(s[5])) &&
        s[6] == '/') {
        char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(s[5])));
        std::string tail = s.substr(7);
        std::replace(tail.begin(), tail.end(), '/', '\\');
        return fs::path(std::string(1, drive) + ":\\" + tail);
    }
    return p;
#else
    // X:\... -> /mnt/x/...
    if (looks_like_windows_abs(p)) {
        char drive = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
        std::string tail = s.substr(2);
        while (!tail.empty() && (tail[0] == '\\' || tail[0] == '/')) {
            tail.erase(tail.begin());
        }
        std::replace(tail.begin(), tail.end(), '\\', '/');
        return fs::path("/mnt") / std::string(1, drive) / tail;
    }
    return p;
#endif
}

// Engine access
extern "C" xbase::XBaseEngine* shell_engine();

static xbase::DbArea& get_area_0based(int slot0) {
    auto* eng = shell_engine();
    if (!eng) throw std::runtime_error("SCHEMAS: engine not available");
    if (slot0 < 0 || slot0 >= xbase::MAX_AREA) throw std::out_of_range("SCHEMAS: area out of range");
    return eng->area(slot0);
}

static int get_area_index(xbase::DbArea& areaRef) {
    auto* eng = shell_engine();
    if (!eng) return -1;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (&eng->area(i) == &areaRef) return i;
    }
    return -1;
}

// Optional alias support
template <typename T>
using has_setLogicalName_t = decltype(std::declval<T&>().setLogicalName(std::declval<std::string>()));

template <typename T, typename = has_setLogicalName_t<T>>
static inline void setLogicalNameIf(T& a, const std::string& s, int) { a.setLogicalName(s); }

template <typename T>
static inline void setLogicalNameIf(T&, const std::string&, long) {}

template <typename T>
using has_setName_t = decltype(std::declval<T&>().setName(std::declval<std::string>()));

template <typename T, typename = has_setName_t<T>>
static inline void setLegacyNameIf(T& a, const std::string& s, int) { a.setName(s); }

template <typename T>
static inline void setLegacyNameIf(T&, const std::string&, long) {}

template <typename T>
using has_name_t = decltype(std::declval<T&>().name());

template <typename T, typename = has_name_t<T>>
static inline std::string getNameIf(T& a, int) { return a.name(); }

template <typename T>
static inline std::string getNameIf(T&, long) { return {}; }

// Optional order/tag support
template <typename Area>
static inline std::string getOrderNameSafe(Area& a) {
    try { return orderstate::orderName(a); } catch (...) { return {}; }
}

template <typename Area>
static inline std::string getActiveTagSafe(Area& a) {
    if constexpr (requires(Area& aa) { orderstate::activeTag(aa); }) {
        try { return orderstate::activeTag(a); } catch (...) {}
    }
    try {
        if (auto* im = a.indexManagerPtr()) return im->activeTag();
    } catch (...) {}
    return {};
}

template <typename Area>
static inline bool setActiveTagSafe(Area& a, const std::string& tag) {
    if (tag.empty() || ci_equal(tag, "none")) return true;
    if constexpr (requires(Area& aa, const std::string& s) { orderstate::setActiveTag(aa, s); }) {
        try { orderstate::setActiveTag(a, tag); return true; } catch (...) {}
    }
    return false;
}

static inline std::string infer_index_type_from_path(const std::string& path) {
    if (path.empty() || ci_equal(path, "none")) return "NONE";
    fs::path p(path);
    if (ieq_ext(p, ".inx")) return "INX";
    if (ieq_ext(p, ".cnx")) return "CNX";
    if (ieq_ext(p, ".cdx")) return "CDX";
    return "UNKNOWN";
}

// Paths helpers
namespace paths = dottalk::paths;

static inline fs::path dbf_root()       { return paths::get_slot(paths::Slot::DBF); }
static inline fs::path idx_root()       { return paths::get_slot(paths::Slot::INDEXES); }
static inline fs::path data_root()      { return paths::get_slot(paths::Slot::DATA); }
static inline fs::path schemas_root()   { return paths::get_slot(paths::Slot::WORKSPACES); }

static inline fs::path resolve_relative_to_root(const fs::path& p) {
    if (p.is_absolute()) return p;
    return fs::weakly_canonical(dbf_root() / p);
}

static inline bool area_open(xbase::DbArea& A) {
    return !A.filename().empty();
}

// --------- OPEN target resolution ------------------------------------------

static bool file_exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && !ec && fs::is_regular_file(p, ec) && !ec;
}

static bool dir_exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && !ec && fs::is_directory(p, ec) && !ec;
}

static fs::path resolve_open_target(const fs::path& raw) {
    if (raw.empty()) return dbf_root();

    if (raw.is_absolute()) return raw;

    const std::string rawStr = s8(raw);
    const std::string rawLow = to_lower(rawStr);
    const fs::path dbfRoot = dbf_root();

    // Existing relative path first.
    if (dir_exists(raw) || file_exists(raw)) return raw;

    // Slot shorthand.
    if (rawLow == "dbf")        return dbfRoot;
    if (rawLow == "data")       return data_root();
    if (rawLow == "indexes")    return idx_root();
    if (rawLow == "schemas")    return paths::get_slot(paths::Slot::SCHEMAS);
    if (rawLow == "scripts")    return paths::get_slot(paths::Slot::SCRIPTS);
    if (rawLow == "tests")      return paths::get_slot(paths::Slot::TESTS);
    if (rawLow == "help")       return paths::get_slot(paths::Slot::HELP);
    if (rawLow == "logs")       return paths::get_slot(paths::Slot::LOGS);
    if (rawLow == "tmp")        return paths::get_slot(paths::Slot::TMP);
    if (rawLow == "workspaces") return paths::get_slot(paths::Slot::WORKSPACES);

    // DBF slot-relative directory/file.
    {
        fs::path cand = dbfRoot / raw;
        if (dir_exists(cand) || file_exists(cand)) return cand;
    }

    // If user passed an index filename, map to DBF stem.
    if (ieq_ext(raw, ".inx") || ieq_ext(raw, ".cnx") || ieq_ext(raw, ".cdx")) {
        fs::path stem = raw.stem();
        fs::path cand = (dbfRoot / stem).concat(".dbf");
        if (file_exists(cand)) return cand;
    }

    // Bare stem conveniences:
    //   <DBF>/<stem>.dbf
    //   <DBF>/<stem>/<stem>.dbf
    if (!raw.has_extension()) {
        fs::path cand1 = (dbfRoot / raw).concat(".dbf");
        if (file_exists(cand1)) return cand1;

        fs::path inner = raw.filename();
        inner.replace_extension(".dbf");
        fs::path cand2 = dbfRoot / raw / inner;
        if (file_exists(cand2)) return cand2;

        fs::path candDir = dbfRoot / raw;
        if (dir_exists(candDir)) return candDir;
    }

    // Final fallback: DBF-slot-relative.
    return dbfRoot / raw;
}

// --------- Index selection --------------------------------------------------

enum class IndexMode { None = 0, ForceCnx, ForceInx, ForceCdx };

static inline std::optional<IndexMode> parse_index_mode_ci(const std::string& token) {
    if (ci_equal(token, "cnx")) return IndexMode::ForceCnx;
    if (ci_equal(token, "inx")) return IndexMode::ForceInx;
    if (ci_equal(token, "cdx")) return IndexMode::ForceCdx;
    return std::nullopt;
}

static std::optional<fs::path> find_index_for_dbf(const fs::path& dbfPath, IndexMode mode, bool fallback) {
    auto file_ok = [](const fs::path& p) {
        std::error_code ec;
        return fs::exists(p, ec) && !ec && fs::is_regular_file(p, ec) && !ec;
    };

    const fs::path stem = fs::path(dbfPath).stem();
    std::string stem_upper = s8(stem);
    for (auto& ch : stem_upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    auto inx_candidates = [&](const fs::path& baseDir) -> std::vector<fs::path> {
        return { (baseDir / stem).concat(".inx") };
    };

    auto cnx_candidates = [&](const fs::path& baseDir) -> std::vector<fs::path> {
        return {
            (baseDir / stem).concat(kCnxPrimaryExt),
            (baseDir / stem).concat(kCnxCompatExt)
        };
    };

    const fs::path sibDir = dbfPath.parent_path().empty() ? fs::current_path() : dbfPath.parent_path();
    const fs::path idxDir = idx_root();

    auto pick_first_existing = [&](const std::vector<fs::path>& cands) -> std::optional<fs::path> {
        for (const auto& p : cands) {
            if (file_ok(p)) return p;
        }
        return std::nullopt;
    };

    auto pick_inx = [&]() -> std::optional<fs::path> {
        if (auto p = pick_first_existing(inx_candidates(sibDir))) return p;
        if (auto p = pick_first_existing(inx_candidates(idxDir))) return p;
        return std::nullopt;
    };

    auto pick_cnx = [&]() -> std::optional<fs::path> {
        if (auto p = pick_first_existing(cnx_candidates(sibDir))) return p;
        if (auto p = pick_first_existing(cnx_candidates(idxDir))) return p;
        return std::nullopt;
    };

    auto pick_cdx = [&](const std::string& stemUpper) -> std::optional<fs::path> {
        fs::path sib_cdx = sibDir / stem;
        sib_cdx.replace_extension(".cdx");

        fs::path idx_cdx = idxDir / stem;
        idx_cdx.replace_extension(".cdx");

        std::vector<fs::path> cdx_candidates = {
            idxDir / (stemUpper + ".cdx"),
            sib_cdx,
            idx_cdx
        };

        for (const auto& p : cdx_candidates) {
            if (fs::exists(p)) return p;
        }
        return std::nullopt;
    };

    if (mode == IndexMode::ForceCdx) {
        if (auto p = pick_cdx(stem_upper)) return p;
        if (fallback) {
            if (auto q = pick_cnx()) return q;
            return pick_inx();
        }
        return std::nullopt;
    }

    if (mode == IndexMode::ForceInx) {
        if (auto p = pick_inx()) return p;
        if (fallback) return pick_cnx();
        return std::nullopt;
    }

    if (mode == IndexMode::ForceCnx) {
        if (auto p = pick_cnx()) return p;
        if (fallback) return pick_inx();
        return std::nullopt;
    }

    return std::nullopt;
}

// --------- OPEN helpers -----------------------------------------------------

struct OpenResult {
    int area = -1;
    fs::path dbf;
    std::optional<fs::path> indexFile;
    bool opened = false;
    bool indexAttached = false;
    string error;
};

#if HAVE_TABLE
static void table_enable_for_area_if_open(int area0) {
    if (area0 < 0 || area0 >= xbase::MAX_AREA) return;
    try {
        auto* eng = shell_engine();
        if (!eng) return;
        if (eng->area(area0).filename().empty()) return;
        dottalk::table::set_enabled(area0, true);
        dottalk::table::set_dirty(area0, false);
        dottalk::table::set_stale(area0, false);
    } catch (...) {}
}

static int table_enable_for_results(const std::vector<OpenResult>& results) {
    int n = 0;
    for (const auto& r : results) {
        if (r.area >= 0 && r.opened) {
            table_enable_for_area_if_open(r.area);
            ++n;
        }
    }
    return n;
}
#endif

static std::vector<OpenResult> schema_open_directory(const fs::path& dir, IndexMode mode, bool fallback) {
    std::vector<OpenResult> results;

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        OpenResult r;
        r.error = "Not a directory: " + s8(dir);
        results.push_back(std::move(r));
        return results;
    }

    std::vector<fs::directory_entry> dbfs;
    for (const auto& de : fs::directory_iterator(dir)) {
        if (is_dbf(de)) dbfs.push_back(de);
    }

    std::sort(dbfs.begin(), dbfs.end(), [](const fs::directory_entry& a, const fs::directory_entry& b){
        auto sa = s8(a.path().filename());
        auto sb = s8(b.path().filename());
        std::transform(sa.begin(), sa.end(), sa.begin(), [](unsigned char c){ return std::tolower(c); });
        std::transform(sb.begin(), sb.end(), sb.begin(), [](unsigned char c){ return std::tolower(c); });
        return sa < sb;
    });

    const int capacity = xbase::MAX_AREA;
    const int toOpen   = static_cast<int>(std::min<size_t>(dbfs.size(), static_cast<size_t>(capacity)));
    const bool overflow = static_cast<int>(dbfs.size()) > capacity;

    for (int area0 = 0; area0 < toOpen; ++area0) {
        const auto& de = dbfs[area0];

        OpenResult r;
        r.area = area0;
        r.dbf = de.path();

        if (mode != IndexMode::None) {
            r.indexFile = find_index_for_dbf(r.dbf, mode, fallback);
        }

        try {
            xbase::DbArea& A = get_area_0based(area0);
            try { orderstate::clearOrder(A); } catch (...) {}
            try { A.close(); } catch (...) {}

            const string dbfStr = s8(r.dbf);
            A.open(dbfStr);
            A.setFilename(dbfStr);

            r.opened = true;

            if (r.indexFile.has_value()) {
                try {
                    orderstate::setOrder(A, s8(*r.indexFile));
                    r.indexAttached = true;
                } catch (...) {
                    r.indexAttached = false;
                }
            }
        } catch (const std::exception& ex) {
            r.error = ex.what();
        } catch (...) {
            r.error = "Unknown error.";
        }

        results.push_back(std::move(r));
    }

    if (overflow) {
        OpenResult r;
        r.area = -1;
        const int skipped = static_cast<int>(dbfs.size()) - capacity;
        r.error = "Exceeded MAX_AREA (" + std::to_string(capacity) + "). Only first " +
                  std::to_string(capacity) + " table(s) opened; " +
                  std::to_string(skipped) + " additional table(s) were skipped.";
        results.push_back(std::move(r));
    }

    return results;
}

static std::vector<OpenResult> schema_open_directory_recursive(const fs::path& dir, IndexMode mode, bool fallback) {
    std::cout << "SCHEMAS OPEN: 'recursive' requested — stubbed; falling back to flat scan.\n";
    return schema_open_directory(dir, mode, fallback);
}

static OpenResult schema_open_single_into_current(xbase::DbArea& current, const fs::path& dbfPath, IndexMode mode, bool fallback) {
    OpenResult r;
    r.dbf = dbfPath;
    r.area = get_area_index(current);

    if (mode != IndexMode::None) {
        r.indexFile = find_index_for_dbf(dbfPath, mode, fallback);
    }

    try {
        try { orderstate::clearOrder(current); } catch (...) {}
        try { current.close(); } catch (...) {}

        const string dbfStr = s8(dbfPath);
        current.open(dbfStr);
        current.setFilename(dbfStr);

        r.opened = true;

        if (r.indexFile.has_value()) {
            try {
                orderstate::setOrder(current, s8(*r.indexFile));
                r.indexAttached = true;
            } catch (...) {
                r.indexAttached = false;
            }
        }
    } catch (const std::exception& ex) {
        r.error = ex.what();
    } catch (...) {
        r.error = "Unknown error.";
    }

    return r;
}

static bool open_into_area(int area0, const fs::path& dbf, const std::optional<fs::path>& index, string* err) {
    try {
        xbase::DbArea& A = get_area_0based(area0);
        try { orderstate::clearOrder(A); } catch (...) {}
        try { A.close(); } catch (...) {}

        const string dbfStr = s8(dbf);
        A.open(dbfStr);
        A.setFilename(dbfStr);

        if (index && !index->empty()) {
            fs::path ip = *index;
            if (!ip.is_absolute()) ip = resolve_relative_to_root(ip);
            if (fs::exists(ip)) {
                try { orderstate::setOrder(A, s8(ip)); } catch (...) {}
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    } catch (...) {
        if (err) *err = "Unknown error.";
        return false;
    }
}

// --------- Printing / List --------------------------------------------------

static void print_open_results(const std::vector<OpenResult>& results) {
    int openedCount = 0;
    int first = -1;
    int last = -1;

    for (const auto& r : results) {
        if (r.area < 0 && !r.error.empty()) {
            std::cout << "  ! " << r.error << "\n";
            continue;
        }

        std::cout << "  Area " << r.area << ": ";
        if (!r.opened) {
            std::cout << "FAILED to open '" << s8(r.dbf.filename()) << "'";
            if (!r.error.empty()) std::cout << " (" << r.error << ")";
            std::cout << "\n";
            continue;
        }

        if (first < 0) first = r.area;
        last = r.area;
        ++openedCount;

        std::cout << "opened '" << s8(r.dbf.filename()) << "'";
        if (r.indexFile.has_value()) {
            std::cout << "  [index: " << s8(r.indexFile->filename())
                      << (r.indexAttached ? ", attached" : ", found (not attached)") << "]";
        }
        std::cout << "\n";
    }

    std::cout << "SCHEMAS: " << openedCount << " table(s) opened";
    if (openedCount > 0) std::cout << " into area(s) " << first << ".." << last;
    const int capacity = xbase::MAX_AREA;
    if (openedCount >= capacity) std::cout << " (capped at MAX_AREA=" << capacity << ")";
    std::cout << ".\n";
}

static void schema_list_open(bool show_all) {
    std::cout << "SCHEMAS: Listing open work areas...\n";

    int open_count = 0;
    for (int area0 = 0; area0 < xbase::MAX_AREA; ++area0) {
        xbase::DbArea& A = get_area_0based(area0);
        if (!A.isOpen()) {
            if (show_all) {
                std::cout << "  Area " << area0 << ": --- closed ---\n";
            }
            continue;
        }
        ++open_count;
        std::cout << "  Area " << area0 << ": " << A.filename() << "\n";
    }

    if (show_all) {
        std::cout << "SCHEMAS: " << open_count << " of " << xbase::MAX_AREA << " area(s) in use.\n";
    } else {
        std::cout << "SCHEMAS: " << open_count << " area(s) open.\n";
    }
}

#if HAVE_RELATIONS
static inline void clear_relations_all_safe() {
    try { relations_api::clear_all_relations(); } catch (...) {}
    try { relations_api::set_current_parent_name(""); } catch (...) {}
}
#else
static inline void clear_relations_all_safe() {}
#endif

// --------- CLOSE helpers ----------------------------------------------------

static bool close_area_if_open(int area0) {
    try {
        xbase::DbArea& A = get_area_0based(area0);
        if (!area_open(A)) return false;

        try { orderstate::clearOrder(A); } catch (...) {}

        try {
            const auto* im = A.indexManagerPtr();
            if (im && im->hasBackend()) {
                A.indexManager().close();
            }
        } catch (...) {}

        try { A.close(); } catch (...) {}
        try { A.setFilename(""); } catch (...) {}

#if HAVE_TABLE
        try {
            dottalk::table::set_enabled(area0, false);
            dottalk::table::set_dirty(area0, false);
            dottalk::table::set_stale(area0, false);
        } catch (...) {}
#endif
        return true;
    } catch (...) {
        return false;
    }
}

static void schema_close_all() {
    std::cout << "SCHEMAS CLOSE: Closing all work areas...\n";
    int close_count = 0;
    for (int area0 = 0; area0 < xbase::MAX_AREA; ++area0) {
        if (close_area_if_open(area0)) close_count++;
    }

#if HAVE_RELATIONS
    clear_relations_all_safe();
#endif

#if HAVE_TABLE
    try { dottalk::table::reset_all(); } catch (...) {}
#endif

    std::cout << "SCHEMAS: " << close_count << " area(s) closed.\n";
}

static int schema_close_matching_token(const string& token) {
    const string t = to_lower(token);
    int close_count = 0;

    for (int area0 = 0; area0 < xbase::MAX_AREA; ++area0) {
        try {
            xbase::DbArea& A = get_area_0based(area0);
            if (!area_open(A)) continue;

            fs::path p = fs::path(A.filename());
            const string full  = to_lower(s8(p));
            const string base  = to_lower(s8(p.filename()));
            const string stem  = to_lower(s8(p.stem()));
            const string alias = to_lower(getNameIf(A, 0));

            if (full == t || base == t || stem == t || (!alias.empty() && alias == t)) {
                if (close_area_if_open(area0)) close_count++;
            }
        } catch (...) {}
    }
    return close_count;
}

// --------- RELATIONS IO (optional) ------------------------------------------

#if HAVE_RELATIONS
static std::vector<string> export_relations_lines() {
    std::vector<string> lines;
    try {
        for (const auto& rs : relations_api::export_relations()) {
            std::ostringstream oss;
            oss << rs.parent << " " << rs.child << " ON ";
            for (size_t i = 0; i < rs.fields.size(); ++i) {
                if (i) oss << ",";
                oss << rs.fields[i];
            }
            lines.push_back(oss.str());
        }
    } catch (...) {}
    return lines;
}
#else
static std::vector<string> export_relations_lines() { return {}; }
#endif

static void apply_relation_line(const std::string& body) {
#if HAVE_RELATIONS
    auto trim_copy_local = [](std::string s) {
        auto is_space = [](unsigned char ch){ return std::isspace(ch) != 0; };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c){ return !is_space(c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c){ return !is_space(c); }).base(), s.end());
        return s;
    };

    auto up_token = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    };

    std::istringstream rss(body);
    std::string parent, child, on;
    rss >> parent >> child >> on;

    if (parent.empty() || child.empty() || up_token(on) != "ON") {
        std::cout << "  ! RELATION skipped (bad syntax): " << body << "\n";
        return;
    }

    std::string fields_csv;
    std::getline(rss, fields_csv);
    fields_csv = trim_copy_local(fields_csv);
    if (fields_csv.empty()) {
        std::cout << "  ! RELATION skipped (no fields): " << body << "\n";
        return;
    }

    std::vector<std::string> fields;
    {
        std::string tok;
        std::istringstream fss(fields_csv);
        while (std::getline(fss, tok, ',')) {
            tok = trim_copy_local(tok);
            if (!tok.empty()) fields.push_back(tok);
        }
    }

    if (fields.empty()) {
        std::cout << "  ! RELATION skipped (no fields): " << body << "\n";
        return;
    }

    if (!relations_api::add_relation(parent, child, fields)) {
        std::cout << "  ! RELATION rejected by engine: " << body << "\n";
    }
#else
    (void)body;
#endif
}

// --------- SAVE / LOAD ------------------------------------------------------

static void schema_save_to_file(const fs::path& file) {
    fs::path outPath = file;
    const fs::path rootSchemas = schemas_root();

    if (outPath.is_relative()) outPath = rootSchemas / outPath;
    if (!outPath.has_extension()) outPath.replace_extension(".dtschema");

    {
        std::error_code ec;
        if (outPath.has_parent_path() && !outPath.parent_path().empty()) {
            fs::create_directories(outPath.parent_path(), ec);
        }
    }

    std::ofstream out(outPath, std::ios::binary);
    if (!out.good()) {
        std::cout << "SCHEMAS SAVE: cannot write file: " << s8(outPath) << "\n";
        return;
    }

    auto weak_can = [](const fs::path& p) -> fs::path {
        std::error_code ec;
        fs::path r = fs::weakly_canonical(p, ec);
        return ec ? p : r;
    };

    auto comp_eq = [](const fs::path& a, const fs::path& b) -> bool {
#if defined(_WIN32)
        return ci_equal(s8(a), s8(b));
#else
        return s8(a) == s8(b);
#endif
    };

    auto is_under = [&](const fs::path& absP, const fs::path& root) -> bool {
        fs::path p = absP;
        fs::path r = root;
        auto pit = p.begin();
        auto rit = r.begin();
        for (; rit != r.end(); ++rit, ++pit) {
            if (pit == p.end()) return false;
            if (!comp_eq(*pit, *rit)) return false;
        }
        return true;
    };

    auto rel_if_under = [&](const fs::path& pIn, const fs::path& root) -> std::string {
        fs::path p = weak_can(pIn);
        fs::path r = weak_can(root);
        if (!r.empty() && p.is_absolute() && is_under(p, r)) {
            fs::path rel = p.lexically_relative(r);
            if (!rel.empty() && rel.native() != p.native()) return s8(rel);
        }
        return s8(p);
    };

    const fs::path rootDbf = dbf_root();
    const fs::path rootIdx = idx_root();

    out << "DTSHEMA 2\n";

    for (int area0 = 0; area0 < xbase::MAX_AREA; ++area0) {
        try {
            xbase::DbArea& A = get_area_0based(area0);
            if (!area_open(A)) continue;

            fs::path dbfPath = fs::path(A.filename());
            std::string index = getOrderNameSafe(A);
            std::string tag   = getActiveTagSafe(A);
            std::string indexType = infer_index_type_from_path(index);
            const std::string alias = getNameIf(A, 0);

            std::string dbfOut = rel_if_under(dbfPath, rootDbf);
            std::string idxOut = index.empty() ? "none" : rel_if_under(fs::path(index), rootIdx);

            out << "AREA " << area0
                << " | dbf="       << dbfOut
                << " | index="     << (idxOut.empty() ? "none" : idxOut)
                << " | indextype=" << (indexType.empty() ? "NONE" : indexType)
                << " | tag="       << (tag.empty() ? "none" : tag);

            if (!alias.empty()) out << " | alias=" << alias;
            out << "\n";
        } catch (...) {}
    }

    for (const auto& rline : export_relations_lines()) {
        out << "RELATION " << rline << "\n";
    }

    out.flush();
    std::cout << "SCHEMAS SAVE: wrote " << s8(outPath) << "\n";
}

static void schema_load_from_file(const fs::path& file) {
    fs::path inPath = file;
    const fs::path rootSchemas = schemas_root();

    if (!inPath.has_extension()) inPath.replace_extension(".dtschema");

    if (inPath.is_relative()) {
        fs::path candidate = rootSchemas / inPath;
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec) inPath = candidate;
        else inPath = fs::current_path() / inPath;
    }

    std::ifstream in(inPath, std::ios::binary);
    if (!in.good()) {
        std::cout << "SCHEMAS LOAD: cannot read file: " << s8(inPath) << "\n";
        return;
    }

    auto weak_can = [](const fs::path& p) -> fs::path {
        std::error_code ec;
        fs::path r = fs::weakly_canonical(p, ec);
        return ec ? p : r;
    };

    const fs::path rootDbf = dbf_root();
    const fs::path rootIdx = idx_root();

    auto resolve_dbf = [&](const fs::path& p) -> fs::path {
        fs::path q = translate_cross_os_absolute(p);
        if (q.is_absolute() || looks_like_windows_abs(q) || looks_like_posix_abs(q)) {
            return weak_can(q);
        }
        return weak_can(rootDbf / q);
    };

    auto resolve_index = [&](const fs::path& p) -> fs::path {
        fs::path q = translate_cross_os_absolute(p);
        if (q.is_absolute() || looks_like_windows_abs(q) || looks_like_posix_abs(q)) {
            return weak_can(q);
        }

        fs::path cand = rootIdx / q;
        std::error_code ec;
        if (fs::exists(cand, ec) && !ec) return weak_can(cand);

        return weak_can(rootDbf / q);
    };

    std::string header;
    std::getline(in, header);
    const std::string headerNorm = to_lower(trim_copy(header));

    int schemaVersion = 0;
    if (headerNorm == "dtshema 1") schemaVersion = 1;
    else if (headerNorm == "dtshema 2") schemaVersion = 2;
    else {
        std::cout << "SCHEMAS LOAD: bad or unsupported file header.\n";
        return;
    }

    schema_close_all();

    std::string line;
    int area_count = 0;
    int relation_count = 0;

    while (std::getline(in, line)) {
        std::string t = trim_copy(line);
        if (t.empty()) continue;

        if (to_lower(t).rfind("area ", 0) == 0) {
            int n = -1;
            {
                std::istringstream ss(t.substr(5));
                ss >> n;
            }

            if (n < 0 || n >= xbase::MAX_AREA) {
                std::cout << "  ! Skip AREA out of range: " << n << "\n";
                continue;
            }

            auto get_field = [&](const char* key) -> std::string {
                auto pos = t.find(std::string(key));
                if (pos == std::string::npos) return {};
                pos += std::char_traits<char>::length(key);
                auto end = t.find('|', pos);
                std::string v = (end == std::string::npos) ? t.substr(pos) : t.substr(pos, end - pos);
                return trim_copy(v);
            };

            fs::path dbf = get_field("dbf=");
            std::string idx = get_field("index=");
            std::string indexType = get_field("indextype=");
            std::string tag = get_field("tag=");
            std::string alias = get_field("alias=");

            if (dbf.empty()) {
                std::cout << "  ! AREA " << n << ": missing dbf path, skipping.\n";
                continue;
            }

            fs::path dbf_resolved = resolve_dbf(dbf);
            std::optional<fs::path> indexPath;
            if (!idx.empty() && to_lower(idx) != "none") {
                indexPath = resolve_index(fs::path(idx));
            }
            if (indexType.empty() && indexPath.has_value()) {
                indexType = infer_index_type_from_path(indexPath->string());
            }
            if (schemaVersion < 2) tag.clear();

            std::string err;
            bool ok = open_into_area(n, dbf_resolved, indexPath, &err);
            if (!ok) {
                std::cout << "  ! AREA " << n << ": open failed (" << err << ")\n";
            } else {
                try {
                    xbase::DbArea& A = get_area_0based(n);
                    if (!alias.empty() && to_lower(alias) != "none") {
                        setLogicalNameIf(A, alias, 0);
                        setLegacyNameIf(A, alias, 0);
                    }
                    if (!tag.empty() && to_lower(tag) != "none") {
                        if (!setActiveTagSafe(A, tag)) {
                            std::cout << "  ! AREA " << n << ": tag '" << tag
                                      << "' could not be activated";
                            if (!indexType.empty()) std::cout << " (type=" << indexType << ")";
                            std::cout << ".\n";
                        }
                    }
                } catch (...) {}
                ++area_count;
            }

        } else if (to_lower(t).rfind("relation ", 0) == 0) {
            std::string body = trim_copy(t.substr(9));
#if HAVE_RELATIONS
            apply_relation_line(body);
            ++relation_count;
#else
            std::cout << "  ~ RELATION ignored (relations module not present): " << body << "\n";
#endif
        } else {
            std::cout << "  ~ Unknown line (ignored): " << t << "\n";
        }
    }

    std::cout << "SCHEMAS LOAD: restored " << area_count << " area(s)";
#if HAVE_RELATIONS
    std::cout << " and " << relation_count << " relation(s)";
#else
    std::cout << " (relations: stubbed)";
#endif
    std::cout << ".\n";
}

// --------- Command Entry ----------------------------------------------------

void cmd_SCHEMAS(xbase::DbArea& current, std::istringstream& in) {
    string arg_line;
    std::getline(in, arg_line);
    string args = trim_copy(arg_line);

    string sub_command, rest_of_args;
    if (args.empty()) {
        sub_command.clear();
        rest_of_args.clear();
    } else {
        auto first_space = args.find_first_of(" \t");
        if (first_space == string::npos) {
            sub_command = trim_copy(args);
            rest_of_args.clear();
        } else {
            sub_command  = trim_copy(args.substr(0, first_space));
            rest_of_args = trim_copy(args.substr(first_space + 1));
        }
    }

    sub_command = to_lower(trim_copy(sub_command));
    rest_of_args = trim_copy(rest_of_args);

    try {
        if (sub_command == "open") {
            auto toks = split_tokens(rest_of_args);

            if (!toks.empty() && (ci_equal(toks[0], "cnx") || ci_equal(toks[0], "inx") ||
                                  ci_equal(toks[0], "idx") || ci_equal(toks[0], "cdx"))) {
                std::cout << "SCHEMAS OPEN: target must come first.\n";
                std::cout << "  Use: SCHEMAS OPEN <target> CNX|INX|CDX [FALLBACK] [recursive] [TABLE]\n";
                return;
            }

            fs::path spec = toks.empty() ? dbf_root() : fs::path(toks[0]);
            bool want_recursive = false;
            bool want_fallback  = false;
            bool want_table     = false;
            IndexMode indexMode = IndexMode::None;

            for (size_t i = 1; i < toks.size(); ++i) {
                const std::string& tok = toks[i];

                if (ci_equal(tok, "idx")) {
                    std::cout << "SCHEMAS OPEN: 'IDX' is not supported. Use CNX, INX, or CDX.\n";
                    return;
                }
                if (parse_recursive_ci(tok)) { want_recursive = true; continue; }
                if (parse_fallback_ci(tok))  { want_fallback  = true; continue; }
                if (parse_table_ci(tok))     { want_table     = true; continue; }

                if (auto m = parse_index_mode_ci(tok)) {
                    indexMode = *m;
                    continue;
                }

                std::cout << "SCHEMAS OPEN: unknown option '" << tok << "' (ignored).\n";
            }

            if (want_fallback && indexMode == IndexMode::None) {
                std::cout << "SCHEMAS OPEN: FALLBACK ignored (CNX/INX/CDX not specified).\n";
                want_fallback = false;
            }

            try {
                auto* eng = shell_engine();
                if (eng && !dottalk::dirty::maybe_prompt_all(*eng, "SCHEMAS OPEN")) {
                    std::cout << "SCHEMAS OPEN canceled.\n";
                    return;
                }
            } catch (...) {}

            schema_close_all();
            spec = resolve_open_target(spec);

            auto mode_tag = [&]() -> const char* {
                if (indexMode == IndexMode::ForceCnx) return "CNX";
                if (indexMode == IndexMode::ForceInx) return "INX";
                if (indexMode == IndexMode::ForceCdx) return "CDX(LMDB)";
                return nullptr;
            };

            if (fs::exists(spec) && fs::is_directory(spec)) {
                std::cout << "SCHEMAS OPEN: scanning directory: " << s8(spec)
                          << (want_recursive ? " (recursive=stub)" : "")
                          << (mode_tag() ? (string(" [") + mode_tag() + "]") : "")
                          << (want_fallback && mode_tag() ? " [FALLBACK]" : "")
                          << (want_table ? " [TABLE]" : "")
                          << "\n";

                auto results = want_recursive
                    ? schema_open_directory_recursive(spec, indexMode, want_fallback)
                    : schema_open_directory(spec, indexMode, want_fallback);

                print_open_results(results);

#if HAVE_TABLE
                if (want_table) {
                    const int n = table_enable_for_results(results);
                    std::cout << "SCHEMAS OPEN: TABLE enabled for " << n << " opened area(s).\n";
                }
#else
                if (want_table) {
                    std::cout << "SCHEMAS OPEN: TABLE requested but table_state module not present.\n";
                }
#endif

            } else if (fs::exists(spec) && fs::is_regular_file(spec) && ieq_ext(spec, ".dbf")) {
                std::cout << "SCHEMAS OPEN: opening single table into current area"
                          << (get_area_index(current) >= 0 ? (" " + std::to_string(get_area_index(current))) : "")
                          << ": " << s8(spec)
                          << (mode_tag() ? (string(" [") + mode_tag() + "]") : "")
                          << (want_fallback && mode_tag() ? " [FALLBACK]" : "")
                          << (want_table ? " [TABLE]" : "")
                          << "\n";

                OpenResult r = schema_open_single_into_current(current, spec, indexMode, want_fallback);
                print_open_results(std::vector<OpenResult>{r});

#if HAVE_TABLE
                if (want_table && r.opened && r.area >= 0) {
                    table_enable_for_area_if_open(r.area);
                    std::cout << "SCHEMAS OPEN: TABLE enabled for area " << r.area << ".\n";
                }
#else
                if (want_table) {
                    std::cout << "SCHEMAS OPEN: TABLE requested but table_state module not present.\n";
                }
#endif

            } else {
                std::cout << "SCHEMAS OPEN: Path not found or unsupported: " << s8(spec) << "\n";
                std::cout << "Usage:\n";
                std::cout << "  SCHEMAS OPEN [<dir>]                      (Open all tables in dir)\n";
                std::cout << "  SCHEMAS OPEN <dir> recursive             (STUB)\n";
                std::cout << "  SCHEMAS OPEN <file.dbf>                  (Open single table in current area)\n";
                std::cout << "  SCHEMAS OPEN <target> CNX [FALLBACK] [recursive] [TABLE]\n";
                std::cout << "  SCHEMAS OPEN <target> INX [FALLBACK] [recursive] [TABLE]\n";
                std::cout << "  SCHEMAS OPEN <target> CDX [FALLBACK] [recursive] [TABLE]   (LMDB)\n";
                std::cout << "Notes:\n";
                std::cout << "  - <target> is always the first argument after OPEN.\n";
                std::cout << "  - Relative targets resolve from SETPATH/INIT slots, primarily DBF.\n";
                std::cout << "  - SCHEMAS OPEN dbf uses the DBF slot directly.\n";
                std::cout << "  - Bare stems like SCHEMAS OPEN students try <DBF>/students.dbf.\n";
                std::cout << "  - Without CNX/INX/CDX, no index files will be attached.\n";
            }

        } else if (sub_command == "close") {
            try {
                auto* eng = shell_engine();
                if (eng && !dottalk::dirty::maybe_prompt_all(*eng, "SCHEMAS CLOSE")) {
                    std::cout << "SCHEMAS CLOSE canceled.\n";
                    return;
                }
            } catch (...) {}

            string tokline = trim_copy(rest_of_args);
            if (tokline.empty()) {
                schema_close_all();
            } else {
                auto tokens = split_tokens(tokline);
                std::unordered_set<int> closed_by_index;
                int total_closed = 0;

                for (const auto& tok : tokens) {
                    int n;
                    if (try_parse_int(tok, n)) {
                        if (n >= 0 && n < xbase::MAX_AREA) {
                            if (!closed_by_index.count(n)) {
                                if (close_area_if_open(n)) {
                                    total_closed++;
                                    closed_by_index.insert(n);
                                }
                            }
                        } else {
                            std::cout << "SCHEMAS CLOSE: Area out of range: " << n
                                      << " (0.." << (xbase::MAX_AREA - 1) << ")\n";
                        }
                    } else {
                        total_closed += schema_close_matching_token(tok);
                    }
                }

#if HAVE_RELATIONS
                if (total_closed > 0) clear_relations_all_safe();
#endif

                if (total_closed == 0) std::cout << "SCHEMAS: No matching open areas to close.\n";
                else std::cout << "SCHEMAS: " << total_closed << " area(s) closed.\n";
            }

        } else if (sub_command == "save") {
            fs::path out = rest_of_args.empty() ? fs::path("session") : fs::path(rest_of_args);
            schema_save_to_file(out);

        } else if (sub_command == "load") {
            try {
                auto* eng = shell_engine();
                if (eng && !dottalk::dirty::maybe_prompt_all(*eng, "SCHEMAS LOAD")) {
                    std::cout << "SCHEMAS LOAD canceled.\n";
                    return;
                }
            } catch (...) {}

            if (rest_of_args.empty()) {
                std::cout << "SCHEMAS LOAD: missing file path.\n";
            } else {
                schema_load_from_file(fs::path(rest_of_args));
            }

        } else if (sub_command == "all") {
            schema_list_open(true);

        } else if (sub_command.empty()) {
            schema_list_open(false);

        } else {
            std::cout << "SCHEMAS: Unknown subcommand '" << sub_command << "'.\n";
            std::cout << "Usage:\n";
            std::cout << "  SCHEMAS                                   (List open areas)\n";
            std::cout << "  SCHEMAS ALL                               (List all areas, including closed slots)\n";
            std::cout << "  SCHEMAS OPEN [<dir>]                      (Open all tables in dir)\n";
            std::cout << "  SCHEMAS OPEN <dir> recursive              (STUB: accepts flag; non-recursive for now)\n";
            std::cout << "  SCHEMAS OPEN <file.dbf>                   (Open single table in current area)\n";
            std::cout << "  SCHEMAS OPEN <target> CNX [FALLBACK] [recursive] [TABLE]\n";
            std::cout << "  SCHEMAS OPEN <target> INX [FALLBACK] [recursive] [TABLE]\n";
            std::cout << "  SCHEMAS OPEN <target> CDX [FALLBACK] [recursive] [TABLE]   (LMDB)\n";
            std::cout << "  SCHEMAS CLOSE                             (Close all open areas)\n";
            std::cout << "  SCHEMAS CLOSE <n> [m ...]                 (Close by index)\n";
            std::cout << "  SCHEMAS CLOSE <name|file|stem|alias>[,...](Close by name/alias; case-insensitive)\n";
            std::cout << "  SCHEMAS SAVE <file>                       (Save areas [+relations if available])\n";
            std::cout << "  SCHEMAS LOAD <file>                       (Load areas [+relations]; relative/cross-OS paths supported)\n";
            std::cout << "Notes:\n";
            std::cout << "  - For OPEN: <target> is always the first argument after OPEN.\n";
            std::cout << "  - Relative targets resolve from SETPATH/INIT slots, primarily DBF.\n";
            std::cout << "  - Bare stems like SCHEMAS OPEN students try <DBF>/students.dbf.\n";
            std::cout << "  - Without CNX/INX/CDX, no index files will be attached.\n";
        }
    } catch (const std::exception& ex) {
        std::cout << "SCHEMAS: Error: " << ex.what() << "\n";
    } catch (...) {
        std::cout << "SCHEMAS: Unknown error.\n";
    }
}