// src/cli/cmd_schema.cpp
// Opens up to MAX_AREA DBF files from <directory> into areas 0..MAX_AREA-1.
// If a same-basename .inx or .cdx is present, attempts to attach it via orderstate.

#include "xbase.hpp"
#include "order_state.hpp"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using std::string;

// NOTE: Removed fixed 25-area cap; now we honor engine's MAX_AREA.
static constexpr bool kPreferInxOverCdx = true; // prefer .inx if both exist

static inline string trim_copy(string s) {
    auto is_space = [](unsigned char ch){ return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](unsigned char c){ return !is_space(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](unsigned char c){ return !is_space(c); }).base(), s.end());
    return s;
}
static inline std::string s8(const fs::path& p) {
#if defined(_WIN32)
    auto u = p.u8string(); return std::string(u.begin(), u.end());
#else
    return p.string();
#endif
}
static inline bool ieq_ext(const fs::path& p, const char* extDotLower) {
    std::string e = p.extension().string();
    const size_t nRef = std::char_traits<char>::length(extDotLower);
    if (e.size() != nRef) return false;
    for (size_t i = 0; i < nRef; ++i) {
        unsigned char a = static_cast<unsigned char>(e[i]);
        unsigned char b = static_cast<unsigned char>(extDotLower[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}
static inline bool is_dbf(const fs::directory_entry& de) {
    return de.is_regular_file() && ieq_ext(de.path(), ".dbf");
}
static std::optional<fs::path> find_sibling_index(const fs::path& dbfPath) {
    fs::path base = dbfPath; base.replace_extension("");
    fs::path inx = base; inx += ".inx";
    fs::path cdx = base; cdx += ".cdx";
    const bool hasInx = fs::exists(inx) && fs::is_regular_file(inx);
    const bool hasCdx = fs::exists(cdx) && fs::is_regular_file(cdx);
    if (hasInx && hasCdx) return kPreferInxOverCdx ? std::optional<fs::path>(inx) : std::optional<fs::path>(cdx);
    if (hasInx) return inx;
    if (hasCdx) return cdx;
    return std::nullopt;
}

// engine access
extern "C" xbase::XBaseEngine* shell_engine();

static xbase::DbArea& get_area_0based(int slot0) {
    auto* eng = shell_engine();
    if (!eng) throw std::runtime_error("SCHEMA: engine not available");
    if (slot0 < 0 || slot0 >= xbase::MAX_AREA) throw std::out_of_range("SCHEMA: area out of range");
    return eng->area(slot0);
}

struct OpenResult {
    int area = -1;                     // 0..capacity-1
    fs::path dbf;
    std::optional<fs::path> indexFile; // .inx or .cdx if present
    bool opened = false;
    bool indexAttached = false;
    string error;
};

static std::vector<OpenResult> open_directory_into_areas(const fs::path& dir) {
    std::vector<OpenResult> results;

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        OpenResult r; r.error = "Not a directory: " + s8(dir);
        results.push_back(std::move(r));
        return results;
    }

    std::vector<fs::directory_entry> dbfs;
    for (const auto& de : fs::directory_iterator(dir)) {
        if (is_dbf(de)) dbfs.push_back(de);
    }
    std::sort(dbfs.begin(), dbfs.end(), [](const fs::directory_entry& a, const fs::directory_entry& b){
        auto sa = s8(a.path().filename()); auto sb = s8(b.path().filename());
        std::transform(sa.begin(), sa.end(), sa.begin(), [](unsigned char c){ return std::tolower(c); });
        std::transform(sb.begin(), sb.end(), sb.begin(), [](unsigned char c){ return std::tolower(c); });
        return sa < sb;
    });

    const int capacity = xbase::MAX_AREA; // honor engine max
    const int toOpen   = static_cast<int>(std::min<size_t>(dbfs.size(), static_cast<size_t>(capacity)));
    const bool overflow = static_cast<int>(dbfs.size()) > capacity;

    for (int area0 = 0; area0 < toOpen; ++area0) {
        const auto& de = dbfs[area0];

        OpenResult r;
        r.area = area0;
        r.dbf = de.path();
        r.indexFile = find_sibling_index(r.dbf);

        try {
            xbase::DbArea& A = get_area_0based(area0);

            try { orderstate::clearOrder(A); } catch (...) {}
            try { A.close(); } catch (...) {}

            const string dbfStr = s8(r.dbf);
            A.open(dbfStr);
            A.setFilename(dbfStr);

            r.opened = true;

            // why: attach sibling index automatically if present
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
        r.error = "Exceeded MAX_AREA (" + std::to_string(capacity) + "). "
                  "Only first " + std::to_string(capacity) + " table(s) opened; "
                  + std::to_string(skipped) + " additional table(s) were skipped.";
        results.push_back(std::move(r));
    }

    return results;
}

// Command entry
void cmd_SCHEMA(xbase::DbArea& /*current*/, std::istringstream& in) {
    string arg; std::getline(in, arg);
    fs::path dir = trim_copy(arg).empty() ? fs::current_path() : fs::path(trim_copy(arg));

    std::cout << "SCHEMA: scanning directory: " << s8(dir) << "\n";

    auto results = open_directory_into_areas(dir);

    int openedCount = 0;
    int first = -1, last = -1;
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

    std::cout << "SCHEMA: " << openedCount << " table(s) opened";
    if (openedCount > 0) std::cout << " into area(s) " << first << ".." << last;
    const int capacity = xbase::MAX_AREA;
    if (openedCount >= capacity) std::cout << " (capped at MAX_AREA=" << capacity << ")";
    std::cout << ".\n";
}
