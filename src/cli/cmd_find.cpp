// src/cli/cmd_find.cpp
//
// FIND <text>
// FIND <field> <text>
//
// Dev-tool contract:
// - If the request targets the active CDX tag, prefer active CDX traversal
//   through the area-local IndexManager.
// - Otherwise, if an order is active, use shared ordered traversal.
// - Otherwise fall back to sequential physical scan.
// - Honors active SET FILTER via filter::visible(&A, nullptr).
// - Does not leave the caller on a different record when search fails or succeeds.

#include <cctype>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "xbase_field_getters.hpp"
#include "cli/order_state.hpp"
#include "cli/order_iterator.hpp"
#include "filters/filter_registry.hpp"
#include "textio.hpp"

namespace {

struct CursorRestore {
    xbase::DbArea* a{nullptr};
    int32_t saved{0};
    bool active{false};

    explicit CursorRestore(xbase::DbArea& area) : a(&area) {
        try {
            saved = area.recno();
            active = (saved >= 1 && saved <= area.recCount());
        } catch (...) {
            active = false;
        }
    }

    ~CursorRestore() {
        if (!active || !a) return;
        try {
            if (a->gotoRec(saved)) {
                (void)a->readCurrent();
            }
        } catch (...) {
        }
    }

    CursorRestore(const CursorRestore&) = delete;
    CursorRestore& operator=(const CursorRestore&) = delete;
};

static std::string trim_copy(std::string s) {
    return textio::trim(s);
}

static std::string upper_copy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool ieq_char(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

static bool ieq(const std::string& a, const char* b) {
    if (!b) return false;
    size_t m = 0;
    while (b[m] != '\0') ++m;
    if (a.size() != m) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!ieq_char(a[i], b[i])) return false;
    }
    return true;
}

static std::string strip_outer_quotes(std::string s) {
    s = trim_copy(std::move(s));
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static bool contains_ci(const std::string& hay, const std::string& needle) {
    const std::string h = upper_copy(hay);
    const std::string n = upper_copy(needle);
    return h.find(n) != std::string::npos;
}

static int find_field_1based(const xbase::DbArea& a, const std::string& name_ci) {
    const auto& fds = a.fields();
    for (size_t i = 0; i < fds.size(); ++i) {
        if (ieq(fds[i].name, name_ci.c_str())) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

struct FindArgs {
    std::string field;
    std::string needle;
    bool ok{false};
};

// FIND <text>
// FIND <field> <text>
// If one token only, use active tag if present, else fail conservatively.
static FindArgs parse_find_args(xbase::DbArea& A, const std::string& rest) {
    FindArgs out{};
    std::istringstream in(rest);

    std::string t1;
    if (!(in >> t1)) return out;

    std::string tail;
    std::getline(in, tail);
    tail = trim_copy(tail);

    if (tail.empty()) {
        const std::string activeTag = orderstate::activeTag(A);
        if (activeTag.empty()) return out;

        out.field = activeTag;
        out.needle = strip_outer_quotes(t1);
        out.ok = !out.field.empty() && !out.needle.empty();
        return out;
    }

    out.field = t1;
    out.needle = strip_outer_quotes(tail);
    out.ok = !out.field.empty() && !out.needle.empty();
    return out;
}

static bool match_find_current(xbase::DbArea& A,
                               int fld,
                               const std::string& needle)
{
    if (!A.readCurrent()) return false;
    if (A.isDeleted()) return false;
    if (!filter::visible(&A, nullptr)) return false;

    try {
        const std::string cur = A.get(fld);
        return contains_ci(cur, needle);
    } catch (...) {
        return false;
    }
}

// Returns true when the CDX route was used, whether or not a match was found.
// found_recno > 0 means success; 0 means routed but not found.
static bool run_find_cdx_active_order(xbase::DbArea& A,
                                      const std::string& field,
                                      const std::string& needle,
                                      int& found_recno)
{
    found_recno = 0;

    if (!A.isOpen()) return false;
    if (!orderstate::hasOrder(A) || !orderstate::isCdx(A)) return false;

    const std::string tagU = upper_copy(orderstate::activeTag(A));
    const std::string fldU = upper_copy(field);
    if (tagU.empty() || tagU != fldU) return false;

    const int fld = find_field_1based(A, field);
    if (fld <= 0) return false;

    const std::string path = orderstate::orderName(A);
    if (path.empty()) return false;

    auto& im = A.indexManager();
    std::string err;

    if (!im.hasBackend() || !im.isCdx() || im.containerPath() != path) {
        if (!im.openCdx(path, tagU, &err)) {
            return false;
        }
    } else {
        if (!im.setTag(tagU, &err)) {
            return false;
        }
    }

    if (!im.hasBackend() || !im.isCdx() || im.activeTag().empty()) {
        return false;
    }

    auto cur = im.scan(xindex::Key{}, xindex::Key{});
    if (!cur) return false;

    const bool asc = orderstate::isAscending(A);

    xindex::Key k;
    xindex::RecNo r = 0;
    bool ok = asc ? cur->first(k, r) : cur->last(k, r);

    while (ok) {
        const int32_t rn = static_cast<int32_t>(r);
        if (rn > 0 && rn <= A.recCount()) {
            try {
                if (A.gotoRec(rn) && match_find_current(A, fld, needle)) {
                    found_recno = rn;
                    return true;
                }
            } catch (...) {
                // continue
            }
        }
        ok = asc ? cur->next(k, r) : cur->prev(k, r);
    }

    return true;
}

// Returns true when ordered route was used, whether or not a match was found.
static bool run_find_ordered_shared(xbase::DbArea& A,
                                    const std::string& field,
                                    const std::string& needle,
                                    int& found_recno)
{
    found_recno = 0;

    if (!A.isOpen()) return false;
    if (!orderstate::hasOrder(A)) return false;

    const int fld = find_field_1based(A, field);
    if (fld <= 0) return false;

    std::vector<uint64_t> recnos;
    cli::OrderIterSpec spec{};
    std::string err;

    if (!cli::order_collect_recnos_asc(A, recnos, &spec, &err)) {
        return false;
    }
    if (recnos.empty()) {
        return true;
    }

    if (spec.ascending) {
        for (uint64_t rn64 : recnos) {
            const int32_t rn = static_cast<int32_t>(rn64);
            if (rn < 1 || rn > A.recCount()) continue;
            if (!A.gotoRec(rn)) continue;
            if (match_find_current(A, fld, needle)) {
                found_recno = rn;
                return true;
            }
        }
    } else {
        for (size_t i = recnos.size(); i-- > 0;) {
            const int32_t rn = static_cast<int32_t>(recnos[i]);
            if (rn < 1 || rn > A.recCount()) continue;
            if (!A.gotoRec(rn)) continue;
            if (match_find_current(A, fld, needle)) {
                found_recno = rn;
                return true;
            }
        }
    }

    return true;
}

// Returns true when physical route was used, whether or not a match was found.
static bool run_find_physical(xbase::DbArea& A,
                              const std::string& field,
                              const std::string& needle,
                              int& found_recno)
{
    found_recno = 0;

    const int fld = find_field_1based(A, field);
    if (fld <= 0) return false;

    if (!A.top()) {
        return true;
    }

    do {
        if (match_find_current(A, fld, needle)) {
            found_recno = A.recno();
            return true;
        }
    } while (A.skip(1));

    return true;
}

} // namespace

void cmd_FIND(xbase::DbArea& A, std::istringstream& args) {
    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    CursorRestore restore(A);

    std::string rest;
    std::getline(args, rest);
    rest = trim_copy(rest);

    const FindArgs fa = parse_find_args(A, rest);
    if (!fa.ok) {
        std::cout << "Usage: FIND <text>  or  FIND <field> <text>\n";
        return;
    }

    // Preferred fast path: active CDX tag search.
    {
        int found_recno = 0;
        const bool routed = run_find_cdx_active_order(A, fa.field, fa.needle, found_recno);
        if (routed) {
            if (found_recno > 0) {
                std::cout << "Found.\n";
            } else {
                std::cout << "Not Found.\n";
            }
            return;
        }
    }

    // Ordered fallback whenever any active order exists.
    if (orderstate::hasOrder(A)) {
        int found_recno = 0;
        const bool routed = run_find_ordered_shared(A, fa.field, fa.needle, found_recno);
        if (routed) {
            if (found_recno > 0) {
                std::cout << "Found.\n";
            } else {
                std::cout << "Not Found.\n";
            }
            return;
        }
    }

    // Physical fallback.
    {
        int found_recno = 0;
        (void)run_find_physical(A, fa.field, fa.needle, found_recno);
        if (found_recno > 0) {
            std::cout << "Found.\n";
        } else {
            std::cout << "Not Found.\n";
        }
    }
}