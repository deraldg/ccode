#include "cli/cmd_sort.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "xbase.hpp"
#include "cli/expr/value_eval.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/ast.hpp"

namespace {

// -------------------- tiny text helpers --------------------

static inline std::string ltrim(std::string s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
static inline std::string rtrim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}
static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }
static inline std::string up(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}
static inline bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
static inline bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}
static inline bool is_simple_ident(const std::string& s) {
    if (s.empty()) return false;
    if (!is_ident_start(s[0])) return false;
    for (size_t i = 1; i < s.size(); ++i) if (!is_ident_char(s[i])) return false;
    return true;
}
static inline std::string rtrim_spaces(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

static std::string strip_outer_quotes(std::string s) {
    s = trim(std::move(s));
    if (s.size() >= 2) {
        const char q = s.front();
        if ((q == '"' || q == '\'') && s.back() == q) return s.substr(1, s.size() - 2);
    }
    return s;
}

// -------------------- ON list splitting (commas at top-level only) --------------------

static std::vector<std::string> split_commas_top_level(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;

    bool in_sq = false;
    bool in_dq = false;
    int  depth = 0;

    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];

        if (c == '\'' && !in_dq) in_sq = !in_sq;
        else if (c == '"' && !in_sq) in_dq = !in_dq;

        if (!in_sq && !in_dq) {
            if (c == '(') depth++;
            else if (c == ')' && depth > 0) depth--;
            else if (c == ',' && depth == 0) {
                out.push_back(trim(cur));
                cur.clear();
                continue;
            }
        }
        cur.push_back(c);
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

static std::pair<std::string, bool> strip_trailing_dir(const std::string& expr) {
    std::string s = trim(expr);

    bool in_sq = false, in_dq = false;
    size_t i = s.size();
    while (i > 0) {
        const char c = s[i - 1];
        if (c == '\'' && !in_dq) in_sq = !in_sq;
        else if (c == '"' && !in_sq) in_dq = !in_dq;

        if (!in_sq && !in_dq && std::isspace(static_cast<unsigned char>(c))) break;
        --i;
    }

    if (i == 0) return {s, false};
    const std::string last = up(trim(s.substr(i)));
    if (last == "ASC" || last == "DESC") {
        std::string base = trim(s.substr(0, i));
        return {base, (last == "DESC")};
    }
    return {s, false};
}

// -------------------- clause splitting (FOR/WHILE/FIELDS/UNIQUE) --------------------

struct ClauseSpans {
    std::string on_list;
    std::string for_expr;
    std::string while_expr;
    std::string fields_list;
    bool unique{false};
};

static bool is_word_boundary(char c) {
    return std::isspace(static_cast<unsigned char>(c)) || c == '\0';
}

static size_t find_keyword_outside_quotes_ci(const std::string& hay,
                                            size_t start,
                                            const std::string& kw_upper) {
    bool in_sq = false;
    bool in_dq = false;

    for (size_t i = start; i < hay.size(); ++i) {
        const char c = hay[i];

        if (c == '\'' && !in_dq) in_sq = !in_sq;
        else if (c == '"' && !in_sq) in_dq = !in_dq;

        if (in_sq || in_dq) continue;

        if (i == 0 || std::isspace(static_cast<unsigned char>(hay[i - 1]))) {
            size_t j = 0;
            for (; j < kw_upper.size() && i + j < hay.size(); ++j) {
                const char a = static_cast<char>(std::toupper(static_cast<unsigned char>(hay[i + j])));
                if (a != kw_upper[j]) break;
            }
            if (j == kw_upper.size()) {
                const char after = (i + j < hay.size()) ? hay[i + j] : '\0';
                if (is_word_boundary(after)) return i;
            }
        }
    }
    return std::string::npos;
}

static ClauseSpans split_on_and_clauses(const std::string& tail_after_on) {
    ClauseSpans cs;
    const std::string s = trim(tail_after_on);

    size_t pos_for    = find_keyword_outside_quotes_ci(s, 0, "FOR");
    size_t pos_while  = find_keyword_outside_quotes_ci(s, 0, "WHILE");
    size_t pos_fields = find_keyword_outside_quotes_ci(s, 0, "FIELDS");
    size_t pos_unique = find_keyword_outside_quotes_ci(s, 0, "UNIQUE");

    auto minpos = [](size_t a, size_t b) {
        if (a == std::string::npos) return b;
        if (b == std::string::npos) return a;
        return (a < b) ? a : b;
    };

    size_t first = std::string::npos;
    first = minpos(first, pos_for);
    first = minpos(first, pos_while);
    first = minpos(first, pos_fields);
    first = minpos(first, pos_unique);

    if (first == std::string::npos) {
        cs.on_list = trim(s);
        return cs;
    }

    cs.on_list = trim(s.substr(0, first));

    size_t cur = first;
    while (cur != std::string::npos && cur < s.size()) {
        auto starts_kw = [&](const char* kw) {
            const std::string K = kw;
            if (cur + K.size() > s.size()) return false;
            for (size_t i = 0; i < K.size(); ++i) {
                if (std::toupper(static_cast<unsigned char>(s[cur + i])) != K[i]) return false;
            }
            const char after = (cur + K.size() < s.size()) ? s[cur + K.size()] : '\0';
            return is_word_boundary(after);
        };

        auto next_any = [&](size_t from) {
            size_t nf = find_keyword_outside_quotes_ci(s, from, "FOR");
            size_t nw = find_keyword_outside_quotes_ci(s, from, "WHILE");
            size_t nfi = find_keyword_outside_quotes_ci(s, from, "FIELDS");
            size_t nu = find_keyword_outside_quotes_ci(s, from, "UNIQUE");
            size_t n = std::string::npos;
            n = minpos(n, nf);
            n = minpos(n, nw);
            n = minpos(n, nfi);
            n = minpos(n, nu);
            return n;
        };

        if (starts_kw("UNIQUE")) {
            cs.unique = true;
            cur += 6;
            cur = next_any(cur);
            continue;
        } else if (starts_kw("FOR")) {
            cur += 3;
            size_t next = next_any(cur);
            cs.for_expr = trim((next == std::string::npos) ? s.substr(cur) : s.substr(cur, next - cur));
            cur = next;
            continue;
        } else if (starts_kw("WHILE")) {
            cur += 5;
            size_t next = next_any(cur);
            cs.while_expr = trim((next == std::string::npos) ? s.substr(cur) : s.substr(cur, next - cur));
            cur = next;
            continue;
        } else if (starts_kw("FIELDS")) {
            cur += 6;
            size_t next = next_any(cur);
            cs.fields_list = trim((next == std::string::npos) ? s.substr(cur) : s.substr(cur, next - cur));
            cur = next;
            continue;
        } else {
            break;
        }
    }

    return cs;
}

// -------------------- field resolution / typed compare --------------------

static int resolve_field_ci(const std::vector<xbase::FieldDef>& F, const std::string& name_in) {
    const std::string want = up(trim(name_in));
    for (int i = 0; i < static_cast<int>(F.size()); ++i) {
        if (up(trim(F[static_cast<size_t>(i)].name)) == want) return i;
    }
    return -1;
}

static double parse_number_or0(std::string s) {
    s = trim(std::move(s));
    if (s.empty()) return 0.0;
    try {
        size_t pos = 0;
        double v = std::stod(s, &pos);
        (void)pos;
        return v;
    } catch (...) {
        return 0.0;
    }
}

static std::int64_t parse_date_yyyymmdd_or0(std::string s) {
    s = trim(std::move(s));
    if (s.size() != 8) return 0;
    std::int64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return 0;
        v = (v * 10) + (c - '0');
    }
    return v;
}

static bool parse_logical(std::string s) {
    s = trim(up(std::move(s)));
    if (s.empty()) return false;
    if (s == ".T." || s == "T" || s == "Y" || s == "1") return true;
    return false;
}

static int cmp_string_ci(const std::string& a, const std::string& b) {
    const size_t na = a.size(), nb = b.size();
    const size_t n = (na < nb) ? na : nb;
    for (size_t i = 0; i < n; ++i) {
        const int ua = std::toupper(static_cast<unsigned char>(a[i]));
        const int ub = std::toupper(static_cast<unsigned char>(b[i]));
        if (ua < ub) return -1;
        if (ua > ub) return  1;
    }
    if (na < nb) return -1;
    if (na > nb) return  1;
    return 0;
}

enum class KeyKind { None, Number, String, Bool, Date };

struct KeyValue {
    KeyKind kind{KeyKind::None};
    std::variant<std::monostate, double, std::string, bool, std::int64_t> v;
};

static int cmp_keyvalue(const KeyValue& a, const KeyValue& b) {
    if (a.kind != b.kind) {
        // Deterministic ordering by kind
        return (static_cast<int>(a.kind) < static_cast<int>(b.kind)) ? -1 : 1;
    }

    switch (a.kind) {
        case KeyKind::None:   return 0;
        case KeyKind::Bool: {
            const bool ba = std::get<bool>(a.v);
            const bool bb = std::get<bool>(b.v);
            if (ba == bb) return 0;
            return ba ? 1 : -1;
        }
        case KeyKind::Number: {
            const double da = std::get<double>(a.v);
            const double db = std::get<double>(b.v);
            if (da < db) return -1;
            if (da > db) return  1;
            return 0;
        }
        case KeyKind::Date: {
            const auto ia = std::get<std::int64_t>(a.v);
            const auto ib = std::get<std::int64_t>(b.v);
            if (ia < ib) return -1;
            if (ia > ib) return  1;
            return 0;
        }
        case KeyKind::String: {
            const auto& sa = std::get<std::string>(a.v);
            const auto& sb = std::get<std::string>(b.v);
            return cmp_string_ci(sa, sb);
        }
    }
    return 0;
}

// -------------------- DBF writer helper --------------------

static void create_empty_dbf_like(const std::filesystem::path& out_path,
                                  const std::vector<xbase::FieldDef>& fields) {
    using namespace xbase;

    int16_t cpr = 1; // delete flag
    for (const auto& f : fields) cpr += static_cast<int16_t>(f.length);

    const int16_t data_start =
        static_cast<int16_t>(sizeof(HeaderRec) + (fields.size() * sizeof(FieldRec)) + 1);

    HeaderRec hdr{};
    hdr.version = 0x03; // dBase III
    {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        tm = *std::localtime(&t);
#endif
        hdr.last_updated[0] = static_cast<uint8_t>(tm.tm_year);
        hdr.last_updated[1] = static_cast<uint8_t>(tm.tm_mon + 1);
        hdr.last_updated[2] = static_cast<uint8_t>(tm.tm_mday);
    }

    hdr.num_of_recs = 0;
    hdr.data_start  = data_start;
    hdr.cpr         = cpr;

    std::vector<FieldRec> frecs(fields.size());
    std::memset(frecs.data(), 0, frecs.size() * sizeof(FieldRec));

    for (size_t i = 0; i < fields.size(); ++i) {
        FieldRec fr{};
        std::memset(&fr, 0, sizeof(fr));

        std::string nm = trim(fields[i].name);
        if (nm.size() > 10) nm.resize(10);
        std::memcpy(fr.field_name, nm.c_str(), nm.size());
        fr.field_name[nm.size()] = '\0';

        fr.field_type         = fields[i].type;
        fr.field_data_address = 0;
        fr.field_length       = fields[i].length;
        fr.decimal_places     = fields[i].decimals;

        frecs[i] = fr;
    }

    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("SORT: cannot create output file: " + out_path.string());

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    out.write(reinterpret_cast<const char*>(frecs.data()),
              static_cast<std::streamsize>(frecs.size() * sizeof(FieldRec)));

    const char term = static_cast<char>(HEADER_TERM_BYTE);
    out.write(&term, 1);

    const char eof = 0x1A;
    out.write(&eof, 1);

    out.flush();
    if (!out) throw std::runtime_error("SORT: failed writing DBF header: " + out_path.string());
}

static std::vector<int> parse_fields_list(const std::vector<xbase::FieldDef>& F, const std::string& fields_list) {
    std::vector<int> idx0;
    const auto parts = split_commas_top_level(fields_list);
    for (const auto& p0 : parts) {
        const std::string p = trim(p0);
        if (p.empty()) continue;
        const int i0 = resolve_field_ci(F, p);
        if (i0 < 0) throw std::runtime_error("SORT: unknown field in FIELDS list: " + p);
        idx0.push_back(i0);
    }
    return idx0;
}

struct KeySpec {
    std::string expr_text;
    bool desc{false};

    bool field_only{false};
    int  field_idx0{-1};
    char field_type{'C'};

    std::unique_ptr<dottalk::expr::Expr> compiled; // compile_where output (moved program)
};

static KeyValue eval_key(xbase::DbArea& A, [[maybe_unused]] const std::vector<xbase::FieldDef>& F, KeySpec& ks) {
    // Field-only => typed by DBF field type
    if (ks.field_only && ks.field_idx0 >= 0) {
        const std::string raw = rtrim_spaces(A.get(ks.field_idx0 + 1));
        switch (ks.field_type) {
            case 'N': case 'F': {
                return {KeyKind::Number, parse_number_or0(raw)};
            }
            case 'D': {
                return {KeyKind::Date, parse_date_yyyymmdd_or0(raw)};
            }
            case 'L': {
                return {KeyKind::Bool, parse_logical(raw)};
            }
            default: {
                return {KeyKind::String, raw};
            }
        }
    }

    // Compiled program => typed result
    if (ks.compiled) {
        auto rv = dottalk::expr::glue::make_record_view(A);
        const auto ev = dottalk::expr::eval_compiled_program(ks.compiled.get(), rv);
        if (ev.kind == dottalk::expr::EvalValue::K_Number) return {KeyKind::Number, ev.number};
        if (ev.kind == dottalk::expr::EvalValue::K_Bool)   return {KeyKind::Bool, ev.tf};
        if (ev.kind == dottalk::expr::EvalValue::K_String) return {KeyKind::String, ev.text};
        return {KeyKind::None, std::monostate{}};
    }

    // Fallback: string value-expr subset
    std::string out;
    if (dottalk::expr::eval_string_value_expr(A, ks.expr_text, out)) {
        return {KeyKind::String, out};
    }
    return {KeyKind::None, std::monostate{}};
}

struct RowKey {
    int32_t recno{0};
    bool    deleted{false};
    std::vector<KeyValue> keys;
};

static bool keys_equal(const std::vector<KeyValue>& a, const std::vector<KeyValue>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (cmp_keyvalue(a[i], b[i]) != 0) return false;
    }
    return true;
}

static void usage_sort() {
    std::cout
        << "SORT [ALL|DELETED] [OVERWRITE] TO <outdbf>\n"
        << "     ON <expr>[ASC|DESC][, <expr>...]\n"
        << "     [FOR <expr>] [WHILE <expr>] [FIELDS <fieldlist>] [UNIQUE]\n";
}

} // namespace

void cmd_SORT(xbase::DbArea& A, std::istringstream& in) {
    if (!A.isOpen()) {
        std::cout << "SORT: no table is open.\n";
        return;
    }

    std::string rest;
    std::getline(in, rest);
    rest = trim(rest);
    if (rest.empty()) { usage_sort(); return; }

    bool include_deleted = false;
    bool only_deleted    = false;
    bool overwrite       = false;

    // Front options
    {
        std::istringstream iss(rest);
        std::string t;
        std::vector<std::string> tokens;
        while (iss >> t) tokens.push_back(t);

        size_t i = 0;
        while (i < tokens.size()) {
            const std::string u = up(tokens[i]);
            if (u == "ALL") { include_deleted = true; only_deleted = false; ++i; continue; }
            if (u == "DELETED") { only_deleted = true; include_deleted = true; ++i; continue; }
            if (u == "OVERWRITE") { overwrite = true; ++i; continue; }
            break;
        }

        std::ostringstream oss;
        for (size_t j = i; j < tokens.size(); ++j) {
            if (j > i) oss << ' ';
            oss << tokens[j];
        }
        rest = trim(oss.str());
    }

    // Find TO and ON
    const size_t pos_to = find_keyword_outside_quotes_ci(rest, 0, "TO");
    const size_t pos_on = find_keyword_outside_quotes_ci(rest, 0, "ON");
    if (pos_to == std::string::npos || pos_on == std::string::npos) { usage_sort(); return; }

    std::string out_name;
    std::string on_and_tail;

    if (pos_to < pos_on) {
        out_name = trim(rest.substr(pos_to + 2, pos_on - (pos_to + 2)));
        on_and_tail = trim(rest.substr(pos_on + 2));
    } else {
        on_and_tail = trim(rest.substr(pos_on + 2, pos_to - (pos_on + 2)));
        out_name = trim(rest.substr(pos_to + 2));
    }

    out_name = trim(out_name);
    if (out_name.empty()) {
        std::cout << "SORT: missing output file after TO.\n";
        return;
    }

    ClauseSpans clauses = split_on_and_clauses(on_and_tail);
    if (trim(clauses.on_list).empty()) {
        std::cout << "SORT: missing ON key list.\n";
        return;
    }

    const auto& F = A.fields();

    // Projection
    std::vector<int> proj_in_idx0;
    if (!clauses.fields_list.empty()) {
        try { proj_in_idx0 = parse_fields_list(F, clauses.fields_list); }
        catch (const std::exception& e) { std::cout << e.what() << "\n"; return; }
    }

    // Parse key specs
    std::vector<KeySpec> keys;
    {
        const auto parts = split_commas_top_level(clauses.on_list);
        for (const auto& p0 : parts) {
            const auto [base0, is_desc] = strip_trailing_dir(p0);
            std::string expr = trim(strip_outer_quotes(base0));
            if (expr.empty()) continue;

            KeySpec ks{};
            ks.expr_text = expr;
            ks.desc = is_desc;

            if (is_simple_ident(expr)) {
                const int idx0 = resolve_field_ci(F, expr);
                if (idx0 >= 0) {
                    ks.field_only = true;
                    ks.field_idx0 = idx0;
                    ks.field_type = F[static_cast<size_t>(idx0)].type;
                }
            }

            // If not field-only, try compile_where once and hold the program
            if (!ks.field_only) {
                std::string err;
                if (dottalk::expr::compile_where_program(expr, ks.compiled, &err)) {
                    // ok
                } else {
                    // leave ks.compiled empty; fallback will be value-expr string
                }
            }

            keys.push_back(std::move(ks));
        }
    }

    if (keys.empty()) {
        std::cout << "SORT: no usable keys found in ON list.\n";
        return;
    }

    // Compile FOR / WHILE (compile once; evaluated per record)
    std::unique_ptr<dottalk::expr::Expr> prog_for;
    std::unique_ptr<dottalk::expr::Expr> prog_while;

    bool have_for = !trim(clauses.for_expr).empty();
    bool have_while = !trim(clauses.while_expr).empty();

    if (have_for) {
        std::string err;
        if (!dottalk::expr::compile_where_program(clauses.for_expr, prog_for, &err)) {
            // Fallback: allow predicate-chain at runtime; if that also fails we'll abort during scan.
            // (We do not abort here because compile_where doesn't always accept chain syntax.)
            prog_for.reset();
        }
    }
    if (have_while) {
        std::string err;
        if (!dottalk::expr::compile_where_program(clauses.while_expr, prog_while, &err)) {
            prog_while.reset();
        }
    }

    auto eval_filter = [&](const std::string& srcText,
                           const std::unique_ptr<dottalk::expr::Expr>& prog,
                           bool& ok) -> bool {
        ok = true;
        const std::string src = trim(srcText);
        if (src.empty()) return true;

        // If we have a compiled program, use it
        if (prog) {
            auto rv = dottalk::expr::glue::make_record_view(A);
            const auto ev = dottalk::expr::eval_compiled_program(prog.get(), rv);
            if (ev.kind == dottalk::expr::EvalValue::K_Bool) return ev.tf;
            if (ev.kind == dottalk::expr::EvalValue::K_Number) return (ev.number != 0.0);
            ok = false;
            return false;
        }

        // Otherwise use full pipeline boolean helper (includes predicate_chain fast path)
        bool b = false;
        std::string err;
        if (!dottalk::expr::eval_bool(A, src, b, &err)) { ok = false; return false; }
        return b;
    };

    // Output schema: full or projection
    std::vector<xbase::FieldDef> out_fields;
    std::vector<int> in_idx0_for_out;

    if (proj_in_idx0.empty()) {
        out_fields = F;
        in_idx0_for_out.resize(static_cast<size_t>(F.size()));
        for (int i = 0; i < static_cast<int>(F.size()); ++i) in_idx0_for_out[static_cast<size_t>(i)] = i;
    } else {
        out_fields.reserve(proj_in_idx0.size());
        in_idx0_for_out.reserve(proj_in_idx0.size());
        for (int idx0 : proj_in_idx0) {
            out_fields.push_back(F[static_cast<size_t>(idx0)]);
            in_idx0_for_out.push_back(idx0);
        }
    }

    std::filesystem::path out_path = xbase::dbNameWithExt(out_name);
    if (std::filesystem::exists(out_path)) {
        if (!overwrite) {
            std::cout << "SORT: output exists (use OVERWRITE): " << out_path.string() << "\n";
            return;
        }
        std::error_code ec;
        std::filesystem::remove(out_path, ec);
        if (ec) {
            std::cout << "SORT: cannot overwrite existing file: " << out_path.string() << "\n";
            return;
        }
    }

    try {
        create_empty_dbf_like(out_path, out_fields);
    } catch (const std::exception& e) {
        std::cout << e.what() << "\n";
        return;
    }

    // Scan + build keys
    std::vector<RowKey> rows;
    rows.reserve(static_cast<size_t>(A.recCount()));

    const int32_t total = A.recCount();
    int32_t scanned = 0;
    int32_t kept    = 0;

    for (int32_t rec = 1; rec <= total; ++rec) {
        if (!A.gotoRec(rec)) continue;
        (void)A.readCurrent();
        ++scanned;

        const bool del = A.isDeleted();
        if (!include_deleted && del) continue;
        if (only_deleted && !del) continue;

        bool ok = true;

        // WHILE: stop scanning at first false
        if (have_while) {
            const bool wb = eval_filter(clauses.while_expr, prog_while, ok);
            if (!ok) { std::cout << "SORT: WHILE evaluation failed.\n"; return; }
            if (!wb) break;
        }

        // FOR: include only when true
        if (have_for) {
            const bool fb = eval_filter(clauses.for_expr, prog_for, ok);
            if (!ok) { std::cout << "SORT: FOR evaluation failed.\n"; return; }
            if (!fb) continue;
        }

        RowKey rk{};
        rk.recno   = rec;
        rk.deleted = del;
        rk.keys.reserve(keys.size());

        for (auto& ks : keys) rk.keys.push_back(eval_key(A, F, ks));

        rows.push_back(std::move(rk));
        ++kept;
    }

    // Sort (stable)
    std::stable_sort(rows.begin(), rows.end(),
        [&](const RowKey& a, const RowKey& b) {
            for (size_t i = 0; i < keys.size(); ++i) {
                const int cmp = cmp_keyvalue(a.keys[i], b.keys[i]);
                if (cmp == 0) continue;
                return keys[i].desc ? (cmp > 0) : (cmp < 0);
            }
            return a.recno < b.recno;
        });

    // Write output, optionally UNIQUE
    try {
        xbase::DbArea out;
        out.open(out_path.string());

        const bool unique = clauses.unique;

        bool have_last = false;
        std::vector<KeyValue> last_keys;

        int32_t written = 0;
        int32_t skipped_dupes = 0;

        for (const auto& rk : rows) {
            if (unique) {
                if (have_last && keys_equal(last_keys, rk.keys)) {
                    ++skipped_dupes;
                    continue;
                }
                last_keys = rk.keys;
                have_last = true;
            }

            if (!A.gotoRec(rk.recno)) continue;
            (void)A.readCurrent();

            if (!out.appendBlank()) {
                throw std::runtime_error("SORT: appendBlank failed while writing output.");
            }

            for (int out_i = 0; out_i < static_cast<int>(in_idx0_for_out.size()); ++out_i) {
                const int in_idx0 = in_idx0_for_out[static_cast<size_t>(out_i)];
                const std::string v = A.get(in_idx0 + 1);
                (void)out.set(out_i + 1, v);
            }
            (void)out.writeCurrent();

            if (rk.deleted) (void)out.deleteCurrent();
            ++written;
        }

        std::cout << "SORT: scanned " << scanned
                  << ", selected " << kept
                  << ", wrote " << written;

        if (unique) std::cout << " (UNIQUE skipped " << skipped_dupes << ")";

        std::cout << " -> " << out_path.string() << "\n";
    } catch (const std::exception& e) {
        std::cout << e.what() << "\n";
        return;
    }
}
