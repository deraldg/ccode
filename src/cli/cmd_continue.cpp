// src/cli/cmd_continue.cpp
//
// CONTINUE
// CONTINUE FOR <expr>
//
// Continues search after the current record.
// If an order is active, traversal follows active order.
// Otherwise traversal is physical forward order.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "cli/order_iterator.hpp"
#include "cli/order_state.hpp"
#include "cli/settings.hpp"
#include "cli/scan_selector.hpp"

namespace locate_continue_bridge {
    void clear();
    void set(const std::string& where_text, bool use_dottalk);
    bool get(std::string& where_text, bool& use_dottalk);
}

namespace {

static std::string trim_copy(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static bool parse_continue_args(std::istringstream& in,
                                std::string& where_text,
                                bool& use_expr)
{
    where_text.clear();
    use_expr = false;

    std::string rest;
    std::getline(in, rest);
    rest = trim_copy(rest);

    if (rest.empty()) {
        return true;
    }

    std::string upper = rest;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper.rfind("FOR ", 0) != 0) {
        return false;
    }

    where_text = trim_copy(rest.substr(4));
    if (where_text.empty()) {
        return false;
    }

    use_expr = true;
    return true;
}

static bool eval_continue_match_current(xbase::DbArea& A,
                                        const std::string& where_text,
                                        bool use_expr)
{
    cli::scan::SelectionSpec spec{};
    spec.scan_mode = cli::scan::ScanMode::Current;

    if (!where_text.empty()) {
        spec.use_expr = true;

        if (use_expr) {
            spec.expr = where_text;
        } else {
            std::string trip = where_text;
            std::string U = where_text;
            for (auto& c : U) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (U.rfind("FOR", 0) != 0) trip = "FOR " + where_text;
            spec.expr = trip;
        }
    }

    return cli::scan::match_current(A, spec);
}

static bool continue_ordered_shared(xbase::DbArea& A,
                                    int32_t start_rec,
                                    const std::string& where_text,
                                    bool use_expr,
                                    int32_t& found_recno)
{
    found_recno = 0;

    std::vector<uint64_t> recnos;
    cli::OrderIterSpec spec{};
    std::string err;

    if (!cli::order_collect_recnos_asc(A, recnos, &spec, &err)) {
        return false;
    }

    if (recnos.empty()) {
        return false;
    }

    auto find_pos = [&](int32_t rec) -> int {
        for (size_t i = 0; i < recnos.size(); ++i) {
            if (static_cast<int32_t>(recnos[i]) == rec) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    const int pos = find_pos(start_rec);

    if (spec.ascending) {
        const size_t begin = (pos >= 0) ? static_cast<size_t>(pos + 1) : 0;

        for (size_t i = begin; i < recnos.size(); ++i) {
            const int32_t rn = static_cast<int32_t>(recnos[i]);
            if (rn <= 0 || rn > A.recCount()) continue;
            if (!A.gotoRec(rn)) continue;
            if (eval_continue_match_current(A, where_text, use_expr)) {
                found_recno = rn;
                return true;
            }
        }
    } else {
        if (pos >= 0) {
            for (size_t i = static_cast<size_t>(pos); i-- > 0;) {
                const int32_t rn = static_cast<int32_t>(recnos[i]);
                if (rn <= 0 || rn > A.recCount()) continue;
                if (!A.gotoRec(rn)) continue;
                if (eval_continue_match_current(A, where_text, use_expr)) {
                    found_recno = rn;
                    return true;
                }
            }
        }
    }

    return false;
}

static bool continue_physical_shared(xbase::DbArea& A,
                                     int32_t start_rec,
                                     const std::string& where_text,
                                     bool use_expr,
                                     int32_t& found_recno)
{
    found_recno = 0;

    for (int32_t rn = start_rec + 1; rn <= A.recCount(); ++rn) {
        if (!A.gotoRec(rn)) continue;
        if (eval_continue_match_current(A, where_text, use_expr)) {
            found_recno = rn;
            return true;
        }
    }

    return false;
}

} // namespace

void cmd_CONTINUE(xbase::DbArea& A, std::istringstream& in)
{
    if (!A.isOpen()) {
        std::cout << "CONTINUE: no file open.\n";
        return;
    }

    if (A.recno() <= 0 || A.recCount() <= 0) {
        std::cout << "CONTINUE: failed.\n";
        return;
    }

    std::string where_text;
    bool use_expr = false;

    if (!parse_continue_args(in, where_text, use_expr)) {
        std::cout << "Usage: CONTINUE [FOR <expr>]\n";
        return;
    }

    // Plain CONTINUE reuses the last successful LOCATE/CONTINUE predicate.
    if (!use_expr) {
        if (!locate_continue_bridge::get(where_text, use_expr)) {
            std::cout << "CONTINUE: no active locate.\n";
            return;
        }
    }

    const int32_t start_rec = A.recno();
    int32_t found_recno = 0;
    bool found = false;

    if (orderstate::hasOrder(A)) {
        found = continue_ordered_shared(A, start_rec, where_text, use_expr, found_recno);
    } else {
        found = continue_physical_shared(A, start_rec, where_text, use_expr, found_recno);
    }

    if (!found || found_recno <= 0) {
        (void)A.gotoRec(start_rec);
        (void)A.readCurrent();
        std::cout << "CONTINUE: not found.\n";
        return;
    }

    if (!A.gotoRec(found_recno) || !A.readCurrent()) {
        std::cout << "CONTINUE: failed.\n";
        return;
    }

    // Keep CONTINUE chain alive.
    locate_continue_bridge::set(where_text, use_expr);

    if (cli::Settings::instance().talk_on.load()) {
        std::cout << "Found at " << A.recno() << ".\n";
    }
}