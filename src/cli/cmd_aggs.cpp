// src/cli/cmd_aggs.cpp
//
// Aggregates: AGGS (dispatcher), plus SUM/AVG/MIN/MAX commands.
//
// Key behavior:
//   • If <value_expr> compiles to a numeric program (LitNumber or Arith), use it.
//   • Else, if <value_expr> is a simple identifier matching a field name,
//     read that field directly and parse as double (FoxPro-ish SUM <field>).
//
// Filtering model:
//   - persistent SET FILTER is part of the logical rowset
//   - optional FOR/WHERE predicate is additionally applied
//   - optional DELETED / NOT DELETED / !DELETED suffix
//
// Cursor cohesion:
//   - save/restore recno()
// Relation refresh suppression during full scan:
//   - shell_rel_refresh_push/pop

#include "cli/cmd_aggs.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cli/expr/api.hpp"         // dottalk::expr::compile_where(...)
#include "cli/expr/ast.hpp"         // Expr, LitNumber, Arith, RecordView
#include "cli/expr/glue_xbase.hpp"  // glue::make_record_view(...)
#include "filters/filter_registry.hpp"

// Must match the linkage used by shell.cpp (see cmd_count.cpp)
extern "C" void shell_rel_refresh_push() noexcept;
extern "C" void shell_rel_refresh_pop() noexcept;

using dottalk::expr::Arith;
using dottalk::expr::Expr;
using dottalk::expr::LitNumber;
using dottalk::expr::RecordView;

namespace {

enum class AggOp { SUM, AVG, MIN, MAX };
enum class DelMode { ALLRECS, ONLY_DELETED, ONLY_NOT_DELETED };

static inline bool is_ws(unsigned char c) { return std::isspace(c) != 0; }

static inline std::string ltrim(std::string s) {
    size_t i = 0;
    while (i < s.size() && is_ws((unsigned char)s[i])) ++i;
    if (i) s.erase(0, i);
    return s;
}
static inline std::string rtrim(std::string s) {
    while (!s.empty() && is_ws((unsigned char)s.back())) s.pop_back();
    return s;
}
static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

static inline std::string upcopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    return s;
}

// Strip inline comments (// or &&) outside quotes.
static std::string strip_line_comments(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    bool in_s = false, in_d = false;

    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        char n = (i + 1 < in.size() ? in[i + 1] : '\0');

        if (!in_s && !in_d) {
            if (c == '/' && n == '/') break;
            if (c == '&' && n == '&') break;
        }
        if (c == '"' && !in_s) { in_d = !in_d; out.push_back(c); continue; }
        if (c == '\'' && !in_d) { in_s = !in_s; out.push_back(c); continue; }

        out.push_back(c);
    }
    return trim(out);
}

static bool boundary_before(const std::string& s, size_t pos) {
    if (pos == 0) return true;
    return is_ws((unsigned char)s[pos - 1]);
}
static bool boundary_after(const std::string& s, size_t pos_end) {
    if (pos_end >= s.size()) return true;
    return is_ws((unsigned char)s[pos_end]);
}

struct KwHit { size_t pos; std::string kw; };

// Find first top-level FOR/WHERE (not in quotes, paren depth 0).
static std::optional<KwHit> find_top_level_for_where(const std::string& s) {
    bool in_s = false, in_d = false;
    int depth = 0;

    auto match_kw = [&](size_t i, const char* kw) -> bool {
        const size_t n = std::char_traits<char>::length(kw);
        if (i + n > s.size()) return false;
        if (!boundary_before(s, i)) return false;
        for (size_t k = 0; k < n; ++k) {
            if ((char)std::toupper((unsigned char)s[i + k]) != kw[k]) return false;
        }
        if (!boundary_after(s, i + n)) return false;
        return true;
    };

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        if (c == '"' && !in_s) { in_d = !in_d; continue; }
        if (c == '\'' && !in_d) { in_s = !in_s; continue; }
        if (in_s || in_d) continue;

        if (c == '(') { ++depth; continue; }
        if (c == ')') { if (depth > 0) --depth; continue; }
        if (depth != 0) continue;

        if (match_kw(i, "FOR"))   return KwHit{i, "FOR"};
        if (match_kw(i, "WHERE")) return KwHit{i, "WHERE"};
    }
    return std::nullopt;
}

// Suffix deleted-mode parser (suffix token/phrase only)
static void extract_deleted_suffix(std::string& s, DelMode& mode_out) {
    mode_out = DelMode::ALLRECS;
    s = trim(std::move(s));
    if (s.empty()) return;

    const std::string U = upcopy(s);

    auto ends_with_phrase = [&](const char* phrase) -> std::optional<size_t> {
        const size_t n = std::char_traits<char>::length(phrase);
        if (U.size() < n) return std::nullopt;
        const size_t start = U.size() - n;
        for (size_t i = 0; i < n; ++i) {
            if (U[start + i] != phrase[i]) return std::nullopt;
        }
        if (!boundary_before(U, start)) return std::nullopt;
        return start;
    };

    if (auto p = ends_with_phrase("NOT DELETED")) {
        mode_out = DelMode::ONLY_NOT_DELETED;
        s = trim(s.substr(0, *p));
        return;
    }
    if (auto p = ends_with_phrase("!DELETED")) {
        mode_out = DelMode::ONLY_NOT_DELETED;
        s = trim(s.substr(0, *p));
        return;
    }
    if (auto p = ends_with_phrase("! DELETED")) {
        mode_out = DelMode::ONLY_NOT_DELETED;
        s = trim(s.substr(0, *p));
        return;
    }
    if (auto p = ends_with_phrase("DELETED")) {
        mode_out = DelMode::ONLY_DELETED;
        s = trim(s.substr(0, *p));
        return;
    }
}

struct AggSpec {
    std::string value_expr; // required
    std::string pred_expr;  // optional
    bool has_pred = false;
    DelMode del_mode = DelMode::ALLRECS;
};

static bool parse_agg_spec(const std::string& raw_tail, AggSpec& out, std::string& err) {
    out = AggSpec{};
    err.clear();

    std::string s = strip_line_comments(raw_tail);
    s = trim(std::move(s));
    if (s.empty()) { err = "missing <value_expr>"; return false; }

    extract_deleted_suffix(s, out.del_mode);
    if (s.empty()) { err = "missing <value_expr>"; return false; }

    if (auto hit = find_top_level_for_where(s)) {
        const size_t kw_pos = hit->pos;
        const size_t kw_len = hit->kw.size();
        out.value_expr = trim(s.substr(0, kw_pos));
        out.pred_expr  = trim(s.substr(kw_pos + kw_len));
        if (out.value_expr.empty()) { err = "missing <value_expr> before FOR/WHERE"; return false; }
        if (out.pred_expr.empty())  { err = "missing predicate after FOR/WHERE"; return false; }
        out.has_pred = true;
        return true;
    }

    out.value_expr = s;
    out.has_pred = false;
    return true;
}

static bool passes_deleted_mode(xbase::DbArea& area, DelMode m) {
    if (m == DelMode::ALLRECS) return true;
    const bool del = area.isDeleted();
    if (m == DelMode::ONLY_DELETED) return del;
    if (m == DelMode::ONLY_NOT_DELETED) return !del;
    return true;
}

// case-insensitive field lookup -> 1-based index
static int field_index_ci(const xbase::DbArea& A, const std::string& name) {
    std::string want = upcopy(trim(name));
    if (want.empty()) return -1;
    try {
        const auto defs = A.fields();
        for (size_t i = 0; i < defs.size(); ++i) {
            if (upcopy(defs[i].name) == want) return (int)i + 1;
        }
    } catch (...) {
        return -1;
    }
    return -1;
}

static bool is_simple_ident(const std::string& s) {
    std::string t = trim(s);
    if (t.empty()) return false;
    for (char c : t) {
        if (std::isalnum((unsigned char)c) || c == '_') continue;
        return false;
    }
    return true;
}

static bool try_parse_double(std::string s, double& out) {
    s = trim(std::move(s));
    if (s.empty()) return false;

    const char* p = s.c_str();
    char* end = nullptr;
    double v = std::strtod(p, &end);
    if (end == p) return false;

    while (*end && std::isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;

    out = v;
    return std::isfinite(out);
}

struct ValuePlan {
    enum Mode { COMPILED_NUMERIC, DIRECT_FIELD, INVALID } mode{INVALID};
    std::string src;
    std::unique_ptr<Expr> prog_owner;
    const Expr* prog = nullptr;
    int field_idx = -1;
    std::string err;
};

static ValuePlan build_value_plan(xbase::DbArea& area, const std::string& value_expr) {
    ValuePlan vp;
    vp.src = trim(value_expr);

    auto cr = dottalk::expr::compile_where(vp.src);
    if (!cr) {
        vp.mode = ValuePlan::INVALID;
        vp.err = cr.error;
        return vp;
    }

    // Numeric program?
    const Expr* p = cr.program.get();
    if (dynamic_cast<const LitNumber*>(p) != nullptr ||
        dynamic_cast<const Arith*>(p) != nullptr) {
        vp.mode = ValuePlan::COMPILED_NUMERIC;
        vp.prog_owner = std::move(cr.program);
        vp.prog = vp.prog_owner.get();
        return vp;
    }

    // Otherwise, treat bare identifier as "field" (Fox SUM <field>)
    if (is_simple_ident(vp.src)) {
        int idx = field_index_ci(area, vp.src);
        if (idx > 0) {
            vp.mode = ValuePlan::DIRECT_FIELD;
            vp.field_idx = idx;
            return vp;
        }
    }

    // Not numeric and not a simple field
    vp.mode = ValuePlan::INVALID;
    vp.err = "value expression is not numeric (try: <field> + 0)";
    return vp;
}

static bool eval_value_plan(ValuePlan& vp, xbase::DbArea& area, const RecordView& rv, double& out) {
    if (vp.mode == ValuePlan::COMPILED_NUMERIC) {
        if (auto ln = dynamic_cast<const LitNumber*>(vp.prog)) {
            out = ln->v;
            return std::isfinite(out);
        }
        if (auto ar = dynamic_cast<const Arith*>(vp.prog)) {
            try {
                out = ar->evalNumber(rv);
                return std::isfinite(out);
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    if (vp.mode == ValuePlan::DIRECT_FIELD) {
        try {
            std::string raw = area.get(vp.field_idx); // 1-based
            return try_parse_double(std::move(raw), out);
        } catch (...) {
            return false;
        }
    }

    return false;
}

static std::shared_ptr<Expr> build_predicate_ast(const AggSpec& spec, std::string& err) {
    err.clear();
    if (!spec.has_pred || spec.pred_expr.empty()) {
        return nullptr;
    }

    auto cr = dottalk::expr::compile_where(spec.pred_expr);
    if (!cr) {
        err = cr.error;
        return nullptr;
    }

    return std::shared_ptr<Expr>(std::move(cr.program));
}

static std::string format_number_classic(double v) {
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << v;
    return oss.str();
}

static void run_agg(AggOp op, const char* opname, xbase::DbArea& area, std::istringstream& args) {
    std::string tail;
    std::getline(args, tail);

    AggSpec spec;
    std::string perr;
    if (!parse_agg_spec(tail, spec, perr)) {
        std::cout << opname << " usage:\n"
                  << " " << opname << " <value_expr> [FOR <pred>] [WHERE <pred>] [DELETED|NOT DELETED|!DELETED]\n";
        return;
    }

    if (!area.isOpen()) {
        if (op == AggOp::SUM) std::cout << "0\n";
        else std::cout << "NULL\n";
        return;
    }

    ValuePlan vp = build_value_plan(area, spec.value_expr);
    if (vp.mode == ValuePlan::INVALID) {
        std::cout << opname << " error: " << vp.err << "\n";
        return;
    }

    std::string ferr;
    std::shared_ptr<Expr> pred_ast = build_predicate_ast(spec, ferr);
    if (spec.has_pred && !pred_ast) {
        std::cout << opname << " error: " << ferr << "\n";
        return;
    }

    const int total = (int)area.recCount();
    if (total <= 0) {
        if (op == AggOp::SUM) std::cout << "0\n";
        else std::cout << "NULL\n";
        return;
    }

    const int saved = area.recno();
    RecordView rv = dottalk::expr::glue::make_record_view(area);

    shell_rel_refresh_push();

    double sum = 0.0;
    uint64_t n = 0;

    bool have_best = false;
    double best = 0.0;

    for (int r = 1; r <= total; ++r) {
        if (!area.gotoRec(r)) continue;
        if (!area.readCurrent()) continue;

        if (!passes_deleted_mode(area, spec.del_mode)) continue;

        // Persistent SET FILTER + optional command FOR/WHERE together define visibility.
        if (!filter::visible(&area, pred_ast)) continue;

        double v = 0.0;
        if (!eval_value_plan(vp, area, rv, v)) continue;

        if (op == AggOp::SUM || op == AggOp::AVG) {
            sum += v;
            ++n;
            continue;
        }

        if (!have_best) {
            best = v;
            have_best = true;
        } else {
            if (op == AggOp::MIN) {
                if (v < best) best = v;
            } else {
                if (v > best) best = v;
            }
        }
    }

    shell_rel_refresh_pop();

    if (saved >= 1 && saved <= total) {
        (void)area.gotoRec(saved);
        (void)area.readCurrent();
    }

    if (op == AggOp::SUM) {
        std::cout << format_number_classic(sum) << "\n";
        return;
    }
    if (op == AggOp::AVG) {
        if (n == 0) std::cout << "NULL\n";
        else std::cout << format_number_classic(sum / (double)n) << "\n";
        return;
    }
    if (!have_best) std::cout << "NULL\n";
    else std::cout << format_number_classic(best) << "\n";
}

} // namespace

void cmd_SUM(xbase::DbArea& area, std::istringstream& args) { run_agg(AggOp::SUM, "SUM", area, args); }
void cmd_AVG(xbase::DbArea& area, std::istringstream& args) { run_agg(AggOp::AVG, "AVG", area, args); }
void cmd_MIN(xbase::DbArea& area, std::istringstream& args) { run_agg(AggOp::MIN, "MIN", area, args); }
void cmd_MAX(xbase::DbArea& area, std::istringstream& args) { run_agg(AggOp::MAX, "MAX", area, args); }

void cmd_AGGS(xbase::DbArea& area, std::istringstream& args) {
    std::string op;
    args >> op;
    op = upcopy(op);

    if (op == "SUM") { run_agg(AggOp::SUM, "AGGS SUM", area, args); return; }
    if (op == "AVG" || op == "AVERAGE") { run_agg(AggOp::AVG, "AGGS AVG", area, args); return; }
    if (op == "MIN") { run_agg(AggOp::MIN, "AGGS MIN", area, args); return; }
    if (op == "MAX") { run_agg(AggOp::MAX, "AGGS MAX", area, args); return; }

    std::cout << "AGGS usage:\n"
              << " AGGS SUM <value_expr> [FOR <pred>] [WHERE <pred>] [DELETED|NOT DELETED|!DELETED]\n"
              << " AGGS AVG <value_expr> [FOR <pred>] [WHERE <pred>] [DELETED|NOT DELETED|!DELETED]\n"
              << " AGGS MIN <value_expr> [FOR <pred>] [WHERE <pred>] [DELETED|NOT DELETED|!DELETED]\n"
              << " AGGS MAX <value_expr> [FOR <pred>] [WHERE <pred>] [DELETED|NOT DELETED|!DELETED]\n";
}