// src/cli/cmd_list_lmdb.cpp
//
// LIST_LMDB / LL
//   Enumerates records in *LMDB index order* (no in-memory sorting).
//
// Contract:
//   DbArea -> IndexManager -> CdxBackend -> LMDB env
//
// Usage (tokens in any order):
//   LL [ALL] [<limit>] [DELETED|NODELETED] [<TAG>]
//   LIST_LMDB ...same...
//
// Notes:
// - If <TAG> is provided, it becomes the active tag for this area (and updates orderstate).
// - Descending order is supported: if the area's order is DESC, we iterate cursor via last/prev.
//
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "textio.hpp"
#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "cli/order_state.hpp"
#include "filters/filter_registry.hpp"

namespace {

struct CursorRestore {
    xbase::DbArea* a{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& area) : a(&area) {
        saved = area.recno();
        active = (saved >= 1 && saved <= area.recCount());
    }

    void cancel() noexcept { active = false; }

    ~CursorRestore() {
        if (!active || !a) return;
        try {
            a->gotoRec(saved);
            a->readCurrent();
        } catch (...) {
        }
    }
};

enum class DelFilter {
    Any,          // default: hide deleted unless ALL
    OnlyDeleted,  // show only deleted
    OnlyAlive     // show only non-deleted
};

enum class DirOverride { None, Asc, Desc };

struct Options {
    bool all{false};
    int  limit{20};             // used only when all==false
    DelFilter del{DelFilter::Any};
    std::string tag_upper{};    // optional: requested tag
    DirOverride dir{DirOverride::None}; // optional: per-command direction override
};

static inline std::string trim_copy(const std::string& s) { return textio::trim(s); }

static inline std::string upper_copy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static inline bool is_digits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c) != 0; });
}

static int recno_width(const xbase::DbArea& a) {
    int n = std::max(1, a.recCount()), w = 0;
    while (n) { n /= 10; ++w; }
    return std::max(3, w);
}

static void header(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    std::cout << "  " << std::setw(recw) << "" << " ";
    for (auto& f : Fs) {
        std::cout << std::left << std::setw((int)f.length) << f.name << " ";
    }
    std::cout << std::right << "\n";
}

static void row(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    std::cout << ' ' << (a.isDeleted() ? '*' : ' ') << ' ' << std::setw(recw) << a.recno() << " ";
    for (int i = 1; i <= (int)Fs.size(); ++i) {
        std::string s = a.get(i);
        int w = (int)Fs[(size_t)(i - 1)].length;
        if ((int)s.size() > w) s.resize((size_t)w);
        std::cout << std::left << std::setw(w) << s << " ";
    }
    std::cout << std::right << "\n";
}

static inline bool pass_deleted_filter(const xbase::DbArea& a, const Options& o) {
    const bool isDel = a.isDeleted();
    switch (o.del) {
        case DelFilter::OnlyDeleted: return isDel;
        case DelFilter::OnlyAlive:   return !isDel;
        case DelFilter::Any:
        default:                     return o.all ? true : !isDel;
    }
}

static Options parse_opts(std::istringstream& iss) {
    Options o{};
    // Accept tokens in any order: ALL, <n>, DELETED, NODELETED, ASC|DESC, <TAG>
    std::string tok;
    while (iss >> tok) {
        if (textio::ieq(tok, "ALL")) {
            o.all = true;
        } else if (textio::ieq(tok, "DELETED")) {
            o.del = DelFilter::OnlyDeleted;
        } else if (textio::ieq(tok, "NODELETED") || textio::ieq(tok, "ALIVE")) {
            o.del = DelFilter::OnlyAlive;
        } else if (is_digits(tok)) {
            const int v = std::stoi(tok);
            if (v > 0) o.limit = v;
        } else if (textio::ieq(tok, "ASC") || textio::ieq(tok, "ASCEND") || textio::ieq(tok, "ASCENDING")) {
            o.dir = DirOverride::Asc;
        } else if (textio::ieq(tok, "DESC") || textio::ieq(tok, "DESCEND") || textio::ieq(tok, "DESCENDING")) {
            o.dir = DirOverride::Desc;
        } else if (o.tag_upper.empty()) {
            o.tag_upper = upper_copy(trim_copy(tok));
        }
    }
    return o;
}

} // namespace

void cmd_LIST_LMDB(xbase::DbArea& a, std::istringstream& iss)
{
    CursorRestore restore(a);

    if (!a.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    Options opt = parse_opts(iss);

    // Determine container + tag from orderstate (canonical for listing commands).
    if (!orderstate::hasOrder(a)) {
        std::cout << "LIST_LMDB: no LMDB order set (use SETLMDB).\n";
        return;
    }

    const std::string container = orderstate::orderName(a); // *.cdx
    std::string tag = !opt.tag_upper.empty() ? opt.tag_upper : orderstate::activeTag(a);

    if (tag.empty()) {
        std::cout << "LIST_LMDB: no active tag.\n";
        return;
    }
    tag = upper_copy(tag);

    // Apply tag override (LL <TAG>) to orderstate + backend.
    if (!opt.tag_upper.empty()) {
        orderstate::setActiveTag(a, tag);
    }

    auto& im = a.indexManager();

    // Ensure correct backend is open for this area/container.
    if (!im.hasBackend() || !im.isCdx() || im.containerPath() != container) {
        std::string err;
        if (!im.openCdx(container, tag, &err)) {
            std::cout << "LIST_LMDB: openCdx failed: " << (err.empty() ? "unknown" : err) << "\n";
            return;
        }
    } else {
        std::string err;
        if (!im.setTag(tag, &err)) {
            std::cout << "LIST_LMDB: setTag failed: " << (err.empty() ? "unknown" : err) << "\n";
            return;
        }
    }

    const int32_t total = a.recCount();
    if (total <= 0) {
        std::cout << "(empty)\n";
        return;
    }

    const int recw = recno_width(a);
    header(a, recw);

    auto cur = im.scan(xindex::Key{}, xindex::Key{});
    if (!cur) {
        std::cout << "LIST_LMDB: cursor open failed.\n";
        return;
    }

    bool asc = orderstate::isAscending(a);
    if (opt.dir == DirOverride::Asc) asc = true;
    else if (opt.dir == DirOverride::Desc) asc = false;

    int printed = 0;
    xindex::Key k;
    xindex::RecNo r;

    bool ok = asc ? cur->first(k, r) : cur->last(k, r);
    while (ok) {
        const int recno = static_cast<int>(r);
        if (recno > 0 && recno <= a.recCount()) {
            if (a.gotoRec(recno) && a.readCurrent()) {

                // Deleted visibility
                if (!pass_deleted_filter(a, opt)) {
                    // skip
                }
                // Active SET FILTER visibility
                else if (!filter::visible(&a, nullptr)) {
                    // skip
                }
                // Emit row
                else {
                    row(a, recw);
                    ++printed;
                    if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
                }
            }
        }
        ok = asc ? cur->next(k, r) : cur->prev(k, r);
    }

    if (!opt.all) {
        std::cout << printed << " record(s) listed (limit " << opt.limit << "). Use LIST_LMDB ALL to show more.\n";
    } else {
        std::cout << printed << " record(s) listed.\n";
    }
}