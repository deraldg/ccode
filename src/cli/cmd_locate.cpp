// src/cli/cmd_locate.cpp

#include <cctype>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "textio.hpp"
#include "predicate_eval.hpp"
#include "xbase_field_getters.hpp"
#include "cli/order_state.hpp"
#include "cli/scan_selector.hpp"
#include "cli/expr/normalize_where.hpp" 


#include "cli/expr/api.hpp"
#include "cli/expr/for_parser.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/text_compare.hpp"

#include "locate_state.hpp"

namespace dottalk { namespace expr { struct CompileResult; } }
bool extract_for_clause(std::istringstream& iss, std::string& out);
dottalk::expr::CompileResult compile_where(const std::string& text);

// -----------------------------------------------------------------------------
// CONTINUE bridge
// -----------------------------------------------------------------------------

namespace locate_continue_bridge {
    static std::string g_where_text;
    static bool g_use_dottalk = false;
    static bool g_active = false;

    void clear() {
        g_where_text.clear();
        g_use_dottalk = false;
        g_active = false;
    }

    void set(const std::string& where_text, bool use_dottalk) {
        g_where_text = where_text;
        g_use_dottalk = use_dottalk;
        g_active = !g_where_text.empty();
    }

    bool get(std::string& where_text, bool& use_dottalk) {
        if (!g_active || g_where_text.empty()) return false;
        where_text = g_where_text;
        use_dottalk = g_use_dottalk;
        return true;
    }
}

// -----------------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------------

static std::string trim(std::string s) {
    auto issp = [](unsigned char c){ return std::isspace(c); };
    while (!s.empty() && issp(s.front())) s.erase(s.begin());
    while (!s.empty() && issp(s.back()))  s.pop_back();
    return s;
}

static std::string upper_copy(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper((unsigned char)c));
    return s;
}

static std::string unquote(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2) {
        char q = s.front();
        if ((q == '"' || q == '\'') && s.back() == q) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return s;
}

static bool looks_complex_where(const std::string& s) {
    std::string U = upper_copy(s);
    return U.find(" AND ") != std::string::npos ||
           U.find(" OR ")  != std::string::npos ||
           U.find('(')     != std::string::npos;
}

// -----------------------------------------------------------------------------
// simple compare
// -----------------------------------------------------------------------------

struct SimpleCmp {
    bool ok{false};
    std::string field;
    std::string op;
    std::string value;
};

static SimpleCmp parse_simple_cmp(const std::string& text) {
    SimpleCmp out{};
    std::istringstream iss(text);

    if (!(iss >> out.field)) return out;
    if (!(iss >> out.op))    return out;

    std::string rest;
    std::getline(iss, rest);
    rest = trim(rest);
    if (rest.empty()) return out;

    out.ok = true;
    out.value = unquote(rest);
    return out;
}

static bool eval_simple_cmp_on_current(xbase::DbArea& A,
                                       const SimpleCmp& sc)
{
    const std::string lhs = trim(xfg::getFieldAsString(A, sc.field));
    const std::string rhs = trim(sc.value);

    const bool case_on = predx::get_case_sensitive();

    const auto match = dottalk::expr::compare_text_values(lhs, rhs);
    return dottalk::expr::text_match_is_true(match, case_on);
}

// -----------------------------------------------------------------------------
// selector
// -----------------------------------------------------------------------------

static bool selector_match_expr_current(xbase::DbArea& A,
                                        const std::string& expr)
{
    cli::scan::SelectionSpec spec{};
    spec.scan_mode = cli::scan::ScanMode::Current;
    spec.use_expr = !expr.empty();
    spec.expr = expr;
    return cli::scan::match_current(A, spec);
}

// -----------------------------------------------------------------------------
// CDX fast path
// -----------------------------------------------------------------------------

static bool try_locate_cdx_simple(xbase::DbArea& A,
                                  const std::string& where_text,
                                  int& found_recno)
{
    found_recno = 0;

    if (!orderstate::hasOrder(A) || !orderstate::isCdx(A)) return false;

    const SimpleCmp sc = parse_simple_cmp(where_text);
    if (!sc.ok) return false;

    if (xfg::resolve_field_index_std(A, sc.field) <= 0) return false;

    const std::string tagU = upper_copy(orderstate::activeTag(A));
    if (tagU != upper_copy(sc.field)) return false;

    auto& im = A.indexManager();
    auto cur = im.scan(xindex::Key{}, xindex::Key{});
    if (!cur) return false;

    xindex::Key k;
    xindex::RecNo r;

    bool ok = orderstate::isAscending(A) ? cur->first(k, r)
                                         : cur->last(k, r);

    while (ok) {
        int32_t rn = static_cast<int32_t>(r);

        if (rn > 0 && rn <= A.recCount()) {
            if (A.gotoRec(rn) && A.readCurrent()) {
                if (selector_match_expr_current(A, "") &&
                    eval_simple_cmp_on_current(A, sc))
                {
                    found_recno = rn;
                    return true;
                }
            }
        }

        ok = orderstate::isAscending(A) ? cur->next(k, r)
                                       : cur->prev(k, r);
    }

    return true;
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

void cmd_LOCATE(xbase::DbArea& A, std::istringstream& iss)
{
    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        return;
    }

    locate_state::clear();
    locate_continue_bridge::clear();

    std::string where_text;
    bool has_where = extract_for_clause(iss, where_text);

    if (!has_where) {
        std::ostringstream os;
        os << iss.rdbuf();
        where_text = trim(os.str());

        if (where_text.empty()) {
            std::cout << "Syntax: LOCATE [FOR <expr>] or LOCATE <field> <op> <value>\n";
            return;
        }
    }

    // ✅ CRITICAL FIX — normalize once here
    where_text = normalize_unquoted_rhs_literals(A, where_text);

    const bool use_dottalk = looks_complex_where(where_text);

    // --- CDX fast path
    if (!use_dottalk) {
        int found_recno = 0;

        if (try_locate_cdx_simple(A, where_text, found_recno)) {
            if (found_recno > 0) {
                std::cout << "Located.\n";
                locate_state::set_after_match(where_text, A.recno(), false);
                locate_continue_bridge::set(where_text, false);
            } else {
                std::cout << "Not Located.\n";
            }
            return;
        }
    }

    // --- fallback scan
    if (!A.top()) {
        std::cout << "Not Located.\n";
        return;
    }

    do {
        if (!A.readCurrent()) continue;

        bool match = selector_match_expr_current(A, where_text);

        if (match) {
            std::cout << "Located.\n";
            locate_state::set_after_match(where_text, A.recno(), use_dottalk);
            locate_continue_bridge::set(where_text, use_dottalk);
            return;
        }

    } while (A.skip(1));

    std::cout << "Not Located.\n";
}