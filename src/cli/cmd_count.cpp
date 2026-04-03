// src/cli/cmd_count.cpp
//
// COUNT — selector-backed, predicate-backed.
// Forms supported:
//   COUNT
//   COUNT FOR <expr>
//   COUNT WHERE <expr>
//   COUNT <expr>          (implicit expression)
//   COUNT DELETED
//   COUNT NOT DELETED
//   COUNT !DELETED
//   COUNT ALL
//
// Notes:
//   • Uses cli::scan::collect_selected_recnos(...) for the common scan/selection path.
//   • Keeps the active-tag simple equality fast path local to COUNT.
//   • Preserves cursor cohesion: COUNT must not leave the work area at EOF/last rec.
//   • Suppresses relations auto-refresh during full scans to prevent refresh thrash.
//   • Persistent SET FILTER is part of the logical rowset.
//     Therefore plain COUNT must NOT use raw recCount() when a persistent filter is active.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "cli/scan_selector.hpp"
#include "cli/order_state.hpp"
#include "cli/order_iterator.hpp"
#include "cli/expr/normalize_where.hpp"
#include "cli/expr/text_compare.hpp"
#include "filters/filter_registry.hpp"
#include "predicate_eval.hpp"

// Provided by src/cli/shell.cpp
extern "C" void shell_rel_refresh_push() noexcept;
extern "C" void shell_rel_refresh_pop() noexcept;

static inline void ltrim_inplace(std::string& s){
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    if (i) s.erase(0, i);
}

static inline void rtrim_inplace(std::string& s){
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
}

static inline std::string trim(std::string s){
    rtrim_inplace(s);
    ltrim_inplace(s);
    return s;
}

static inline std::string upcopy(std::string s){
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static inline bool starts_with_i(const std::string& s, const char* kw){
    const size_t n = std::char_traits<char>::length(kw);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i){
        if ((char)std::toupper((unsigned char)s[i]) !=
            (char)std::toupper((unsigned char)kw[i])) return false;
    }
    return true;
}

// Conservative inline comment stripper
static inline std::string strip_inline_comments(std::string s){
    const auto pos_dblamp = s.find("&&");
    const auto pos_slash  = s.find(" //");

    size_t cut = std::string::npos;
    if (pos_dblamp != std::string::npos) cut = pos_dblamp;
    if (pos_slash  != std::string::npos) cut = (cut == std::string::npos ? pos_slash : std::min(cut, pos_slash));

    if (cut != std::string::npos) s.erase(cut);
    return trim(s);
}

enum class CountMode {
    ALLRECS,
    ONLY_DELETED,
    ONLY_NOT_DELETED,
    EXPR
};

struct CountSpec {
    CountMode mode{CountMode::ALLRECS};
    std::string expr;
};

struct CountFastPath {
    bool usable{false};
    std::string field;
    std::string literal;
};

static bool strip_matching_quotes(std::string& s){
    s = trim(std::move(s));
    if (s.size() >= 2) {
        const char a = s.front();
        const char b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s = s.substr(1, s.size() - 2);
            return true;
        }
    }
    return false;
}

static int find_field_index_ci(const xbase::DbArea& area, const std::string& name){
    const auto& defs = area.fields();
    const std::string needle = upcopy(trim(name));

    for (size_t i = 0; i < defs.size(); ++i) {
        if (upcopy(defs[i].name) == needle) {
            return static_cast<int>(i) + 1; // 1-based
        }
    }
    return 0;
}

static CountFastPath try_parse_simple_eq_fast_path(const CountSpec& spec){
    CountFastPath fp;
    if (spec.mode != CountMode::EXPR) return fp;

    std::string expr = trim(spec.expr);
    if (expr.empty()) return fp;

    // Reject boolean logic for v1 fast path.
    const std::string up = upcopy(" " + expr + " ");
    if (up.find(" AND ") != std::string::npos ||
        up.find(" OR ")  != std::string::npos ||
        up.find(" NOT ") != std::string::npos) {
        return fp;
    }

    const auto pos = expr.find('=');
    if (pos == std::string::npos) return fp;

    // Reject <=, >=, ==, != in v1.
    if ((pos > 0 && (expr[pos - 1] == '<' || expr[pos - 1] == '>' || expr[pos - 1] == '!' || expr[pos - 1] == '=')) ||
        (pos + 1 < expr.size() && expr[pos + 1] == '=')) {
        return fp;
    }

    std::string lhs = trim(expr.substr(0, pos));
    std::string rhs = trim(expr.substr(pos + 1));
    if (lhs.empty() || rhs.empty()) return fp;

    strip_matching_quotes(rhs);

    fp.usable  = true;
    fp.field   = upcopy(lhs);
    fp.literal = rhs;
    return fp;
}

static bool active_tag_matches_field(xbase::DbArea& area, const CountFastPath& fp){
    if (!fp.usable) return false;
    if (!orderstate::hasOrder(area)) return false;

    const std::string tag = upcopy(trim(orderstate::activeTag(area)));
    if (tag.empty() || tag == "(NONE)") return false;

    return tag == fp.field;
}

static uint64_t do_count_fast_eq_active_tag(xbase::DbArea& area,
                                            const CountFastPath& fp)
{
    uint64_t matched = 0;
    std::vector<uint64_t> recnos;
    cli::OrderIterSpec spec{};
    std::string err;

    if (!cli::order_collect_recnos_asc(area, recnos, &spec, &err) || recnos.empty()) {
        return 0;
    }

    const int field_idx = find_field_index_ci(area, fp.field);
    if (field_idx <= 0) {
        return 0;
    }

    const bool case_on = predx::get_case_sensitive();

    for (uint64_t rn : recnos) {
        if (!area.gotoRec((int32_t)rn)) continue;
        if (!area.readCurrent()) continue;

        // Keep fast path logically consistent with COUNT selection semantics.
        if (!filter::visible(&area, nullptr)) continue;

        const std::string val = trim(area.get(field_idx));
        const auto match = dottalk::expr::compare_text_values(val, fp.literal);
        if (dottalk::expr::text_match_is_true(match, case_on)) {
            ++matched;
        }
    }
    return matched;
}

static CountSpec parse_count_tail(std::string tail_raw){
    CountSpec cs;
    const std::string tail = strip_inline_comments(trim(std::move(tail_raw)));
    const std::string up   = upcopy(tail);

    if (up.empty() || up == "ALL"){
        cs.mode = CountMode::ALLRECS;
        return cs;
    }
    if (up == "DELETED"){
        cs.mode = CountMode::ONLY_DELETED;
        return cs;
    }
    if (up == "NOT DELETED" || up == "!DELETED" || up == "! DELETED"){
        cs.mode = CountMode::ONLY_NOT_DELETED;
        return cs;
    }
    if (starts_with_i(up, "FOR ")){
        cs.mode = CountMode::EXPR;
        cs.expr = trim(tail.substr(4));
        return cs;
    }
    if (starts_with_i(up, "WHERE ")){
        cs.mode = CountMode::EXPR;
        cs.expr = trim(tail.substr(6));
        return cs;
    }

    cs.mode = CountMode::EXPR; // implicit expression
    cs.expr = tail;
    return cs;
}

static inline bool count_requires_scan(const CountSpec& spec, bool has_persistent_filter)
{
    if (has_persistent_filter) return true;
    if (spec.mode == CountMode::ONLY_DELETED) return true;
    if (spec.mode == CountMode::ONLY_NOT_DELETED) return true;
    if (spec.mode == CountMode::EXPR && !spec.expr.empty()) return true;
    return false;
}

static cli::scan::SelectionSpec to_selection_spec(const CountSpec& spec)
{
    cli::scan::SelectionSpec ss{};
    ss.scan_mode = cli::scan::ScanMode::All;
    ss.next_n = 0;
    ss.ordered_snapshot = true;

    switch (spec.mode) {
        case CountMode::ONLY_DELETED:
            ss.deleted_mode = cli::scan::DeletedMode::OnlyDeleted;
            break;
        case CountMode::ONLY_NOT_DELETED:
            ss.deleted_mode = cli::scan::DeletedMode::OnlyAlive;
            break;
        case CountMode::ALLRECS:
        case CountMode::EXPR:
        default:
            ss.deleted_mode = cli::scan::DeletedMode::UseDefault;
            break;
    }

    if (spec.mode == CountMode::EXPR && !spec.expr.empty()) {
        ss.use_expr = true;
        ss.expr = spec.expr;
    }

    return ss;
}

void cmd_COUNT(xbase::DbArea& area, std::istringstream& args)
{
    if (!area.isOpen()){
        std::cout << "0\n";
        return;
    }

    std::string tail;
    std::getline(args, tail);
    CountSpec spec = parse_count_tail(tail);

    if (spec.mode == CountMode::EXPR && !spec.expr.empty()) {
        spec.expr = normalize_unquoted_rhs_literals(area, spec.expr);
    }

    const bool has_persistent_filter = filter::has_active_filter(&area);
    const bool will_scan = count_requires_scan(spec, has_persistent_filter) || spec.mode == CountMode::ALLRECS;

    // Bookmark current cursor state.
    const uint64_t saved_recno = static_cast<uint64_t>(area.recno());
    const uint64_t total       = static_cast<uint64_t>(area.recCount());

    if (will_scan) {
        shell_rel_refresh_push();
    }

    uint64_t result = 0;

    if (spec.mode == CountMode::ALLRECS && !has_persistent_filter) {
        // True fast-path only when there is no persistent SET FILTER.
        result = static_cast<uint64_t>(area.recCount());
    } else {
        // Keep COUNT's local optimization.
        const CountFastPath fp = try_parse_simple_eq_fast_path(spec);
        if (active_tag_matches_field(area, fp)) {
            result = do_count_fast_eq_active_tag(area, fp);
        } else {
            const cli::scan::SelectionSpec sel_spec = to_selection_spec(spec);
            const cli::scan::SelectionResult sel = cli::scan::collect_selected_recnos(area, sel_spec);
            result = static_cast<uint64_t>(sel.recnos.size());
        }
    }

    if (will_scan) {
        shell_rel_refresh_pop();

        if (saved_recno >= 1 && saved_recno <= total) {
            (void)area.gotoRec((int32_t)saved_recno);
            (void)area.readCurrent();
        }
    }

    std::cout << result << "\n";
}