// src/cli/cmd_replace_multi.cpp — multi-field replace with one record lock + one write
//
// CLI usage:
//   REPLACE_MULTI <field> WITH <value>[, <field> WITH <value>]...
//
// Also provides a programmatic overload:
//   bool cmd_REPLACE_MULTI(xbase::DbArea& A,
//                          const std::vector<FieldUpdate>& updates,
//                          std::string* error)
//
// Rule (current direct-write phase):
//   - Writes directly to DBF => does NOT mark DIRTY.
//   - If a field actually changes, attempt immediate index maintenance through
//     IndexManager simple-field tag logic.
//   - Only mark field-level STALE if index maintenance fails for that changed field.
//
// Notes:
//   - Buffering/COMMIT/ROLLBACK are deferred.
//   - Compound/computed tags are deferred.
//   - Simple first-pass policy: field-name == tag-name via IndexManager.

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xbase.hpp"
#include "xbase_locks.hpp"

#include "cli/table_state.hpp"
#include "cli/cli_currency_pair.hpp"
#include "memo/memo_auto.hpp"

// Added: direct index maintenance seam
#include "xindex/index_manager.hpp"

// Provided by the interactive shell.
extern "C" xbase::XBaseEngine* shell_engine(void);

// --------- If you already have FieldUpdate in a header, include it instead ----------
struct FieldUpdate {
    std::string name;   // field name OR numeric string index (e.g. "3")
    std::string value;  // raw value
};
// -----------------------------------------------------------------------------------

namespace {

static std::string up_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static void trim_inplace(std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a])) ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    s = s.substr(a, b - a);
}

static std::string trim_copy(std::string s) {
    trim_inplace(s);
    return s;
}

static bool try_parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    for (; i < s.size(); ++i) {
        if (!std::isdigit((unsigned char)s[i])) return false;
    }
    try {
        long long v = std::stoll(s);
        if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) return false;
        out = (int)v;
        return true;
    } catch (...) {
        return false;
    }
}

static bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

static bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

// Resolve a field name (case-insensitive) to 1-based index.
static int resolve_field_index_by_name_ci(xbase::DbArea& A, const std::string& name) {
    try {
        const std::string want = up_copy(name);
        const auto defs = A.fields();
        for (std::size_t i = 0; i < defs.size(); ++i) {
            if (up_copy(defs[i].name) == want) return (int)i + 1;
        }
    } catch (...) {}
    return 0;
}

static bool values_equal_for_stale_check(const std::string& before, const std::string& after) {
    return before == after;
}

static char field_type_upper(xbase::DbArea& A, int fld1) {
    try {
        const auto defs = A.fields();
        const int idx0 = fld1 - 1;
        if (idx0 < 0 || idx0 >= (int)defs.size()) return '\0';
        return (char)std::toupper((unsigned char)defs[(std::size_t)idx0].type);
    } catch (...) {
        return '\0';
    }
}

static int field_length(xbase::DbArea& A, int fld1) {
    try {
        const auto defs = A.fields();
        const int idx0 = fld1 - 1;
        if (idx0 < 0 || idx0 >= (int)defs.size()) return 0;
        return (int)defs[(std::size_t)idx0].length;
    } catch (...) {
        return 0;
    }
}

static int field_decimals(xbase::DbArea& A, int fld1) {
    try {
        const auto defs = A.fields();
        const int idx0 = fld1 - 1;
        if (idx0 < 0 || idx0 >= (int)defs.size()) return 0;
        return (int)defs[(std::size_t)idx0].decimals;
    } catch (...) {
        return 0;
    }
}

static bool is_memo_field_type(xbase::DbArea& A, int fld1) {
    const char t = field_type_upper(A, fld1);
    return t == 'M';
}

static bool parse_i32_strict(const std::string& s, std::int32_t& out) {
    const std::string t = trim_copy(s);
    if (t.empty()) return false;

    const char* first = t.data();
    const char* last  = t.data() + t.size();

    std::int32_t v = 0;
    auto r = std::from_chars(first, last, v);
    if (r.ec != std::errc() || r.ptr != last) return false;

    out = v;
    return true;
}

static bool parse_double_strict(const std::string& s, double& out) {
    const std::string t = trim_copy(s);
    if (t.empty()) return false;

    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);

    if (end == t.c_str()) return false;
    if (*end != '\0') return false;
    if (errno == ERANGE) return false;
    if (!std::isfinite(v)) return false;

    out = v;
    return true;
}

static bool parse_currency_strict(const std::string& s, std::int64_t& out_scaled) {
    double v = 0.0;
    if (!parse_double_strict(s, v)) return false;

    const double scaled = v * 10000.0;
    if (!std::isfinite(scaled)) return false;

    const double lo = static_cast<double>(std::numeric_limits<std::int64_t>::min());
    const double hi = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (scaled < lo || scaled > hi) return false;

    out_scaled = static_cast<std::int64_t>(std::llround(scaled));
    return true;
}

static bool is_valid_yyyymmdd(const std::string& s) {
    if (s.size() != 8) return false;
    for (char c : s) {
        if (!std::isdigit((unsigned char)c)) return false;
    }

    const int y = std::stoi(s.substr(0, 4));
    const int m = std::stoi(s.substr(4, 2));
    const int d = std::stoi(s.substr(6, 2));

    if (m < 1 || m > 12) return false;

    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int maxd = mdays[m - 1];

    if (m == 2) {
        const bool leap = (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0));
        if (leap) maxd = 29;
    }

    return d >= 1 && d <= maxd;
}

static std::string today_yyyymmdd() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif

    char buf[9];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d",
                  tmv.tm_year + 1900,
                  tmv.tm_mon + 1,
                  tmv.tm_mday);
    return std::string(buf);
}

static bool normalize_date_value(const std::string& raw, std::string& out) {
    std::string s = trim_copy(raw);
    const std::string u = up_copy(s);

    if (u == "TODAY") {
        out = today_yyyymmdd();
        return true;
    }

    if (is_valid_yyyymmdd(s)) {
        out = s;
        return true;
    }

    if (s.size() == 10 && s[2] == '/' && s[5] == '/') {
        const std::string mm = s.substr(0, 2);
        const std::string dd = s.substr(3, 2);
        const std::string yy = s.substr(6, 4);
        const std::string canon = yy + mm + dd;
        if (is_valid_yyyymmdd(canon)) {
            out = canon;
            return true;
        }
    }

    return false;
}

static bool normalize_logical_value(const std::string& raw, std::string& out) {
    const std::string s = up_copy(trim_copy(raw));

    if (s == "T" || s == ".T." || s == "TRUE" || s == "Y" || s == "1") {
        out = "T";
        return true;
    }

    if (s == "F" || s == ".F." || s == "FALSE" || s == "N" || s == "0") {
        out = "F";
        return true;
    }

    return false;
}

static bool normalize_numeric_value(const std::string& raw,
                                    int flen,
                                    int fdec,
                                    std::string& out)
{
    std::string s = trim_copy(raw);
    if (s.empty()) return false;

    bool seen_dot = false;
    int digits_before = 0;
    int digits_after = 0;
    std::size_t i = 0;

    if (s[i] == '+' || s[i] == '-') ++i;
    if (i >= s.size()) return false;

    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (std::isdigit((unsigned char)c)) {
            if (seen_dot) ++digits_after;
            else ++digits_before;
            continue;
        }
        if (c == '.' && !seen_dot) {
            seen_dot = true;
            continue;
        }
        return false;
    }

    if (digits_before == 0 && digits_after == 0) return false;

    if (fdec <= 0) {
        if (seen_dot) return false;
        const int sign_chars = (!s.empty() && (s[0] == '+' || s[0] == '-')) ? 1 : 0;
        if ((digits_before + sign_chars) > flen) return false;
        out = s;
        return true;
    }

    if (digits_after > fdec) return false;

    const int sign_chars = (!s.empty() && (s[0] == '+' || s[0] == '-')) ? 1 : 0;
    const int total_chars = digits_before + sign_chars + 1 + digits_after;
    if (total_chars > flen) return false;

    out = s;
    return true;
}

static bool validate_new_scalar_field_value(xbase::DbArea& A,
                                            int fld1,
                                            const std::string& value,
                                            std::string& err)
{
    const char t = field_type_upper(A, fld1);

    switch (t) {
        case 'D': {
            std::string norm;
            if (!normalize_date_value(value, norm)) {
                err = "REPLACE_MULTI: invalid date for field.";
                return false;
            }
            return true;
        }

        case 'L': {
            std::string norm;
            if (!normalize_logical_value(value, norm)) {
                err = "REPLACE_MULTI: invalid logical for field.";
                return false;
            }
            return true;
        }

        case 'N': {
            std::string norm;
            if (!normalize_numeric_value(value, field_length(A, fld1), field_decimals(A, fld1), norm)) {
                err = "REPLACE_MULTI: invalid numeric for field.";
                return false;
            }
            return true;
        }

        case 'F': {
            std::string norm;
            if (!normalize_numeric_value(value, field_length(A, fld1), field_decimals(A, fld1), norm)) {
                err = "REPLACE_MULTI: invalid float for field.";
                return false;
            }
            return true;
        }

        case 'I': {
            std::int32_t tmp = 0;
            if (!parse_i32_strict(value, tmp)) {
                err = "REPLACE_MULTI: invalid int32 for field.";
                return false;
            }
            return true;
        }

        case 'B': {
            double tmp = 0.0;
            if (!parse_double_strict(value, tmp)) {
                err = "REPLACE_MULTI: invalid double for field.";
                return false;
            }
            return true;
        }

        case 'Y': {
            std::int64_t scaled = 0;
            if (!parse_currency_strict(value, scaled)) {
                err = "REPLACE_MULTI: invalid currency for field.";
                return false;
            }
            return true;
        }

        default:
            return true;
    }
}

// Parse: <field> WITH <value>[, <field> WITH <value>]...
static bool parse_assignments(std::istringstream& iss,
                              xbase::DbArea& A,
                              std::vector<FieldUpdate>& out,
                              std::string& err)
{
    out.clear();

    while (true) {
        iss >> std::ws;
        if (!iss.good()) break;

        std::string fieldTok;
        if (!(iss >> fieldTok)) break;

        if (up_copy(fieldTok) == "FIELD") {
            if (!(iss >> fieldTok)) {
                err = "REPLACE_MULTI: expected field after FIELD.";
                return false;
            }
        }

        std::string withTok;
        if (!(iss >> withTok) || up_copy(withTok) != "WITH") {
            err = "REPLACE_MULTI: expected WITH after field.";
            return false;
        }

        std::string value;
        std::getline(iss, value, ',');
        trim_inplace(value);
        if (value.empty()) {
            err = "REPLACE_MULTI: empty value.";
            return false;
        }

        trim_inplace(fieldTok);

        int dummy = 0;
        if (!try_parse_int(fieldTok, dummy)) {
            if (fieldTok.empty() || !is_ident_start(fieldTok[0])) {
                err = "REPLACE_MULTI: invalid field token '" + fieldTok + "'.";
                return false;
            }
            for (char c : fieldTok) {
                if (!is_ident_char(c)) {
                    err = "REPLACE_MULTI: invalid field token '" + fieldTok + "'.";
                    return false;
                }
            }
        }

        int fld = 0;
        if (!try_parse_int(fieldTok, fld)) fld = resolve_field_index_by_name_ci(A, fieldTok);
        if (fld <= 0) {
            err = "REPLACE_MULTI: unknown field '" + fieldTok + "'.";
            return false;
        }

        out.push_back({fieldTok, value});

        if (!iss.good()) break;
    }

    if (out.empty()) {
        err = "REPLACE_MULTI: no assignments.";
        return false;
    }
    return true;
}

} // namespace

bool cmd_REPLACE_MULTI(xbase::DbArea& A,
                       const std::vector<FieldUpdate>& updates,
                       std::string* error)
{
    if (!A.isOpen()) {
        if (error) *error = "REPLACE_MULTI: no file open.";
        return false;
    }

    const auto rn = static_cast<uint32_t>(A.recno());
    if (rn == 0) {
        if (error) *error = "REPLACE_MULTI: no current record.";
        return false;
    }

    struct Resolved {
        int field1{};
        std::string value;       // raw user value
        std::string storeValue;  // normalized value actually written
        std::string before;
        bool isMemo{false};
    };

    std::vector<Resolved> resolved;
    resolved.reserve(updates.size());

    for (const auto& u : updates) {
        int fld = 0;
        if (!try_parse_int(u.name, fld)) {
            fld = resolve_field_index_by_name_ci(A, u.name);
        }
        if (fld <= 0) {
            if (error) *error = "REPLACE_MULTI: unknown field '" + u.name + "'.";
            return false;
        }

        Resolved r;
        r.field1 = fld;
        r.value = u.value;
        r.storeValue = u.value;
        r.isMemo = is_memo_field_type(A, fld);
        try { r.before = A.get(fld); } catch (...) { r.before.clear(); }
        resolved.push_back(std::move(r));
    }

    std::string lock_err;
    if (!xbase::locks::try_lock_record(A, rn, &lock_err)) {
        if (error) *error = "REPLACE_MULTI: record is locked (" + lock_err + ").";
        return false;
    }

    bool ok = true;
    std::string local_error;

    try {
        // Pass 1: validate everything and compute normalized store values.
        for (auto& r : resolved) {
            if (r.isMemo) {
                r.storeValue = r.value;
                continue;
            }

            if (!validate_new_scalar_field_value(A, r.field1, r.value, local_error)) {
                ok = false;
                break;
            }

            r.storeValue = r.value;

            const char t = field_type_upper(A, r.field1);

            if (t == 'D') {
                std::string norm;
                if (!normalize_date_value(r.value, norm)) {
                    local_error = "REPLACE_MULTI: invalid date for field.";
                    ok = false;
                    break;
                }
                r.storeValue = norm;
            }
            else if (t == 'L') {
                std::string norm;
                if (!normalize_logical_value(r.value, norm)) {
                    local_error = "REPLACE_MULTI: invalid logical for field.";
                    ok = false;
                    break;
                }
                r.storeValue = norm;
            }
            else if (t == 'N') {
                std::string norm;
                if (!normalize_numeric_value(r.value,
                                             field_length(A, r.field1),
                                             field_decimals(A, r.field1),
                                             norm)) {
                    local_error = "REPLACE_MULTI: invalid numeric for field.";
                    ok = false;
                    break;
                }
                r.storeValue = norm;
            }
            else if (t == 'F') {
                std::string norm;
                if (!normalize_numeric_value(r.value,
                                             field_length(A, r.field1),
                                             field_decimals(A, r.field1),
                                             norm)) {
                    local_error = "REPLACE_MULTI: invalid float for field.";
                    ok = false;
                    break;
                }
                r.storeValue = norm;
            }

            std::string normCur;
            std::string curErr;
            if (!cli_currency::validate_and_normalize_currency_pair_field(
                    A, r.field1, r.storeValue, normCur, curErr)) {
                local_error = "REPLACE_MULTI: " + curErr + ".";
                ok = false;
                break;
            }
            r.storeValue = normCur;
        }

        if (ok) {
            // Pass 2: apply all changes to the in-memory current record.
            for (auto& r : resolved) {
                if (r.isMemo) {
                    auto* store = cli_memo::memo_store_for(A);
                    if (!store || !store->is_open()) {
                        local_error = "REPLACE_MULTI: memo backend not attached.";
                        ok = false;
                        break;
                    }

                    dottalk::memo::MemoRef old_ref{};
                    try {
                        old_ref.token = A.get(r.field1);
                    } catch (...) {
                        old_ref.token.clear();
                    }

                    dottalk::memo::MemoPutResult mr =
                        store->is_null_ref(old_ref)
                            ? store->put_text(r.value)
                            : store->update_text(old_ref, r.value);

                    if (!mr.ok) {
                        local_error = "REPLACE_MULTI: memo write failed";
                        if (!mr.error.empty()) local_error += " (" + mr.error + ")";
                        ok = false;
                        break;
                    }

                    r.storeValue = mr.ref.token;

                    if (!A.set(r.field1, r.storeValue)) {
                        local_error = "REPLACE_MULTI: failed to store memo token in DBF field.";
                        ok = false;
                        break;
                    }
                } else {
                    if (!A.set(r.field1, r.storeValue)) {
                        local_error = "REPLACE_MULTI: field set failed.";
                        ok = false;
                        break;
                    }
                }
            }
        }

        if (ok) ok = A.writeCurrent();
    } catch (...) {
        ok = false;
        if (local_error.empty()) local_error = "REPLACE_MULTI: exception during write.";
    }

    xbase::locks::unlock_record(A, rn);

    if (!ok) {
        if (error) {
            *error = local_error.empty() ? "REPLACE_MULTI: write failed." : local_error;
        }
        return false;
    }

    // Direct-write CRUD index maintenance for simple field-named tags.
    // Policy:
    // - If field did not actually change: do nothing.
    // - If field changed: attempt immediate index maintenance through IndexManager.
    // - Only mark STALE if index maintenance fails for that changed field.
    if (auto* eng = shell_engine()) {
        const int area0 = eng->currentArea();
        auto& im = A.indexManager();

        for (const auto& r : resolved) {
            std::string after;
            try { after = A.get(r.field1); } catch (...) { after.clear(); }

            if (values_equal_for_stale_check(r.before, after)) {
                continue; // no real change
            }

            bool idx_ok = false;
            try {
                idx_ok = im.replace_active_field_value(
                    r.field1,
                    r.before,
                    after,
                    static_cast<xindex::RecNo>(rn));
            } catch (...) {
                idx_ok = false;
            }

            if (!idx_ok) {
                // Keep stale semantics narrow: only unresolved index-affecting changes
                // should stay marked stale in this direct-write phase.
                dottalk::table::mark_stale_field(area0, r.field1);
            }
        }
    }

    return true;
}

void cmd_REPLACE_MULTI(xbase::DbArea& A, std::istringstream& iss)
{
    std::string err;
    std::vector<FieldUpdate> updates;

    if (!parse_assignments(iss, A, updates, err)) {
        std::cout << err << "\n";
        std::cout << "Usage: REPLACE_MULTI <field> WITH <value>[, <field> WITH <value>]...\n";
        return;
    }

    if (!cmd_REPLACE_MULTI(A, updates, &err)) {
        std::cout << err << "\n";
        return;
    }

    std::cout << "REPLACE_MULTI: updated " << updates.size() << " field(s).\n";
}