// src/cli/cmd_calcwrite.cpp
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <ctime>

#include "xbase.hpp"
#include "xbase_64.hpp"
#include "xbase_locks.hpp"

#include "cli/expr/rhs_eval.hpp"
#include "cli/expr/value_eval.hpp"
#include "cli/expr/fn_date.hpp"
#include "cli/expr/fn_string.hpp"

#include "value_normalize.hpp"

#include "cli/settings.hpp"
#include "cli/table_state.hpp"
#include "cli/cli_currency_pair.hpp"

#include "memo/memo_auto.hpp"
#include "memo/memostore.hpp"

// Provided by the interactive shell.
extern "C" xbase::XBaseEngine* shell_engine(void);

using cli::Settings;

namespace {

static inline std::string ltrim(std::string s) {
    size_t i = 0;
    while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
    return s.substr(i);
}
static inline std::string rtrim(std::string s) {
    if (s.empty()) return s;
    size_t i = s.size();
    while (i > 0 && std::isspace((unsigned char)s[i - 1])) --i;
    s.resize(i);
    return s;
}
static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

static inline std::string upper(std::string s) {
    std::transform(
        s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::toupper(c); });
    return s;
}

static bool parse_assignment(const std::string& line, std::string& out_field, std::string& out_expr) {
    bool in_s = false, in_d = false;
    int depth = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];

        if (c == '"' && !in_s) { in_d = !in_d; continue; }
        if (c == '\'' && !in_d) { in_s = !in_s; continue; }
        if (in_s || in_d) continue;

        if (c == '(') { ++depth; continue; }
        if (c == ')') { if (depth > 0) --depth; continue; }

        if (depth == 0 && c == '=') {
            const char prev = (i > 0) ? line[i - 1] : ' ';
            const char next = (i + 1 < line.size()) ? line[i + 1] : ' ';
            if (prev == '<' || prev == '>' || prev == '!' || prev == '=' || next == '=') continue;

            out_field = trim(line.substr(0, i));
            out_expr  = trim(line.substr(i + 1));

            if (out_field.empty() || out_expr.empty()) return false;

            for (char fc : out_field) {
                if (!(std::isalnum((unsigned char)fc) || fc == '_' || fc == '$')) return false;
            }
            return true;
        }
    }

    return false;
}

static int field_index_ci(const xbase::DbArea& a, std::string_view name) {
    const auto& Fs = a.fields();
    std::string U  = upper(std::string(name));
    for (int i = 0; i < (int)Fs.size(); ++i) {
        std::string H = Fs[(size_t)i].name;
        if (upper(H) == U) return i + 1;
    }
    return 0;
}

static int resolve_current_index(xbase::DbArea& A) {
    xbase::XBaseEngine* eng = shell_engine();
    if (!eng) return -1;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (&eng->area(i) == &A) return i;
    }
    return -1;
}

// ------------------------------
// X64 memo helpers
// ------------------------------
static bool is_x64_memo_field(const xbase::DbArea& A, int field1) {
    if (field1 < 1 || field1 > A.fieldCount()) return false;
    if (A.versionByte() != xbase::DBF_VERSION_64) return false;

    const auto& f = A.fields()[static_cast<std::size_t>(field1 - 1)];
    return (f.type == 'M' || f.type == 'm') &&
           f.length == xbase::X64_MEMO_FIELD_LEN;
}

static char field_type_upper(const xbase::DbArea& A, int field1) {
    if (field1 < 1 || field1 > A.fieldCount()) return '\0';
    return (char)std::toupper((unsigned char)A.fields()[(std::size_t)(field1 - 1)].type);
}

static int field_length(const xbase::DbArea& A, int field1) {
    if (field1 < 1 || field1 > A.fieldCount()) return 0;
    return (int)A.fields()[(std::size_t)(field1 - 1)].length;
}

static int field_decimals(const xbase::DbArea& A, int field1) {
    if (field1 < 1 || field1 > A.fieldCount()) return 0;
    return (int)A.fields()[(std::size_t)(field1 - 1)].decimals;
}

static std::uint64_t parse_u64_or_zero(const std::string& s) {
    if (s.empty()) return 0;
    try {
        std::size_t used = 0;
        const unsigned long long v = std::stoull(s, &used, 10);
        if (used != s.size()) return 0;
        return static_cast<std::uint64_t>(v);
    } catch (...) {
        return 0;
    }
}

static dottalk::memo::MemoStore* memo_store_for_area(xbase::DbArea& A) noexcept {
    auto* backend = cli_memo::memo_backend_for(A);
    if (!backend) return nullptr;
    return dynamic_cast<dottalk::memo::MemoStore*>(backend);
}

static std::string get_logical_field_text(xbase::DbArea& area, int field1) {
    if (field1 <= 0) return {};
    try {
        if (!is_x64_memo_field(area, field1)) {
            return area.get(field1);
        }

        const std::uint64_t oid = parse_u64_or_zero(area.get(field1));
        if (!oid) return {};

        auto* store = memo_store_for_area(area);
        if (!store) return {};

        std::string txt;
        if (!store->get_text_id(oid, txt, nullptr)) return {};
        return txt;
    } catch (...) {
        return {};
    }
}

static bool build_x64_memo_stored_value(xbase::DbArea& A,
                                        int field1,
                                        const std::string& user_value,
                                        std::string& stored_value_out,
                                        std::string& err_out)
{
    stored_value_out.clear();
    err_out.clear();

    if (!is_x64_memo_field(A, field1)) {
        stored_value_out = user_value;
        return true;
    }

    auto* store = memo_store_for_area(A);
    if (!store) {
        err_out = "memo backend not attached";
        return false;
    }

    std::uint64_t old_object_id = 0;
    try {
        old_object_id = parse_u64_or_zero(A.get(field1));
    } catch (...) {
        old_object_id = 0;
    }

    if (user_value.empty()) {
        stored_value_out.clear();
        return true;
    }

    std::uint64_t new_object_id = 0;
    if (!store->update_text_id(old_object_id,
                               std::string_view(user_value),
                               new_object_id,
                               nullptr))
    {
        err_out = "memo store update failed";
        return false;
    }

    if (new_object_id == 0) stored_value_out.clear();
    else stored_value_out = std::to_string(new_object_id);

    return true;
}

static bool parse_i32_strict(const std::string& s, std::int32_t& out) {
    const std::string t = trim(s);
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
    const std::string t = trim(s);
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
    for (char c : s)
        if (!std::isdigit((unsigned char)c)) return false;

    int y = std::stoi(s.substr(0,4));
    int m = std::stoi(s.substr(4,2));
    int d = std::stoi(s.substr(6,2));

    if (m < 1 || m > 12) return false;

    static const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int maxd = mdays[m-1];

    if (m == 2) {
        bool leap = (y%4==0)&&((y%100!=0)||(y%400==0));
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
        tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday);
    return std::string(buf);
}

static bool normalize_date_value(const std::string& raw, std::string& out) {
    std::string s = trim(raw);
    std::string u = upper(s);

    if (u == "TODAY") {
        out = today_yyyymmdd();
        return true;
    }

    if (is_valid_yyyymmdd(s)) {
        out = s;
        return true;
    }

    if (s.size()==10 && s[2]=='/' && s[5]=='/') {
        std::string canon = s.substr(6,4)+s.substr(0,2)+s.substr(3,2);
        if (is_valid_yyyymmdd(canon)) {
            out = canon;
            return true;
        }
    }

    return false;
}

static bool normalize_logical_value(const std::string& raw, std::string& out) {
    std::string s = upper(trim(raw));

    if (s=="T"||s==".T."||s=="TRUE"||s=="Y"||s=="1") { out="T"; return true; }
    if (s=="F"||s==".F."||s=="FALSE"||s=="N"||s=="0") { out="F"; return true; }

    return false;
}

static bool normalize_numeric_value(const std::string& raw,
                                    int flen,
                                    int fdec,
                                    std::string& out)
{
    std::string s = trim(raw);
    if (s.empty()) return false;

    bool dot=false;
    int before=0, after=0;
    size_t i=0;

    if (s[i]=='+'||s[i]=='-') ++i;
    if (i>=s.size()) return false;

    for (; i<s.size(); ++i) {
        char c=s[i];
        if (std::isdigit((unsigned char)c)) {
            dot ? ++after : ++before;
        }
        else if (c=='.' && !dot) dot=true;
        else return false;
    }

    if (before==0 && after==0) return false;
    if (fdec<=0 && dot) return false;
    if (after > fdec) return false;

    int sign = (s[0]=='+'||s[0]=='-') ? 1 : 0;
    int total = before + sign + (dot?1:0) + after;
    if (total > flen) return false;

    out = s;
    return true;
}

static bool validate_field_value_for_store(const xbase::DbArea& A,
                                           int field1,
                                           const std::string& stored_value,
                                           std::string& err_out)
{
    err_out.clear();
    const char t = field_type_upper(A, field1);

    std::string norm;

    switch (t) {
        case 'D':
            if (!normalize_date_value(stored_value, norm)) {
                err_out = "invalid date for field";
                return false;
            }
            return true;

        case 'L':
            if (!normalize_logical_value(stored_value, norm)) {
                err_out = "invalid logical for field";
                return false;
            }
            return true;

        case 'N':
            if (!normalize_numeric_value(stored_value,
                                         field_length(A, field1),
                                         field_decimals(A, field1),
                                         norm)) {
                err_out = "invalid numeric for field";
                return false;
            }
            return true;

        case 'F':
            if (!normalize_numeric_value(stored_value,
                                         field_length(A, field1),
                                         field_decimals(A, field1),
                                         norm)) {
                err_out = "invalid float for field";
                return false;
            }
            return true;

        case 'I': {
            std::int32_t tmp;
            if (!parse_i32_strict(stored_value, tmp)) {
                err_out = "invalid int32 for field";
                return false;
            }
            return true;
        }

        case 'B': {
            double tmp;
            if (!parse_double_strict(stored_value, tmp)) {
                err_out = "invalid double for field";
                return false;
            }
            return true;
        }

        case 'Y': {
            std::int64_t tmp;
            if (!parse_currency_strict(stored_value, tmp)) {
                err_out = "invalid currency for field";
                return false;
            }
            return true;
        }

        default:
            return true;
    }
}

struct EvalResult {
    enum Kind { K_None, K_Number, K_String, K_Bool } kind = K_None;
    double number = 0.0;
    std::string text;
    bool tf = false;
};

static std::string format_date8_from_number(double dv) {
    long long iv = static_cast<long long>(dv + (dv >= 0 ? 0.5 : -0.5));
    std::string s = std::to_string(iv);

    if (s.size() < 8) s = std::string(8 - s.size(), '0') + s;
    if (s.size() > 8) s.resize(8);
    return s;
}

static std::string to_string_trim(double dv, int fdec) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(std::max(0, fdec)) << dv;
    std::string s = trim(os.str());
    while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s.empty() ? "0" : s;
}

// Less coercive candidate builder for CALCWRITE.
// This creates a candidate string, then normalization/validation decides final legality.
static std::string build_candidate_for_field(char ftype, int flen, int fdec, const EvalResult& er) {
    std::string out;

    switch ((char)std::toupper((unsigned char)ftype)) {
    case 'D':
        if (er.kind == EvalResult::K_Number)
            out = format_date8_from_number(er.number);
        else if (er.kind == EvalResult::K_String)
            out = er.text;
        else if (er.kind == EvalResult::K_Bool)
            out = er.tf ? "T" : "F";
        break;

    case 'L':
        if (er.kind == EvalResult::K_Bool)
            out = er.tf ? "T" : "F";
        else if (er.kind == EvalResult::K_Number)
            out = (er.number != 0.0) ? "1" : "0";
        else if (er.kind == EvalResult::K_String)
            out = er.text;
        break;

    case 'N':
        if (er.kind == EvalResult::K_Number)
            out = to_string_trim(er.number, fdec);
        else if (er.kind == EvalResult::K_Bool)
            out = er.tf ? "1" : "0";
        else if (er.kind == EvalResult::K_String)
            out = er.text;
        break;

    case 'F':
        if (er.kind == EvalResult::K_Number)
            out = to_string_trim(er.number, fdec);
        else if (er.kind == EvalResult::K_Bool)
            out = er.tf ? "1" : "0";
        else if (er.kind == EvalResult::K_String)
            out = er.text;
        break;

    case 'I':
        if (er.kind == EvalResult::K_Number) {
            long long iv = static_cast<long long>(er.number + (er.number >= 0 ? 0.5 : -0.5));
            out = std::to_string(iv);
        } else if (er.kind == EvalResult::K_Bool) {
            out = er.tf ? "1" : "0";
        } else if (er.kind == EvalResult::K_String) {
            out = trim(er.text);
        } else {
            out = "0";
        }
        break;

    case 'B':
        if (er.kind == EvalResult::K_Number)
            out = to_string_trim(er.number, 10);
        else if (er.kind == EvalResult::K_Bool)
            out = er.tf ? "1" : "0";
        else if (er.kind == EvalResult::K_String)
            out = trim(er.text);
        break;

    case 'Y':
        if (er.kind == EvalResult::K_Number)
            out = to_string_trim(er.number, 4);
        else if (er.kind == EvalResult::K_Bool)
            out = er.tf ? "1.0000" : "0.0000";
        else if (er.kind == EvalResult::K_String)
            out = trim(er.text);
        break;

    default:
        if (er.kind == EvalResult::K_String)
            out = er.text;
        else if (er.kind == EvalResult::K_Number)
            out = to_string_trim(er.number, fdec);
        else if (er.kind == EvalResult::K_Bool)
            out = er.tf ? "T" : "F";
        else
            out.clear();
        break;
    }

    if (flen >= 0 && (int)out.size() > flen) out.resize((size_t)flen);
    return out;
}

static bool normalize_calcwrite_value_for_field(char ftype,
                                                int flen,
                                                int fdec,
                                                const std::string& candidate,
                                                std::string& normalized,
                                                std::string& err_out)
{
    err_out.clear();
    normalized = candidate;

    switch ((char)std::toupper((unsigned char)ftype)) {
        case 'D':
            if (!normalize_date_value(candidate, normalized)) {
                err_out = "invalid date for field";
                return false;
            }
            return true;

        case 'L':
            if (!normalize_logical_value(candidate, normalized)) {
                err_out = "invalid logical for field";
                return false;
            }
            return true;

        case 'N':
            if (!normalize_numeric_value(candidate, flen, fdec, normalized)) {
                err_out = "invalid numeric for field";
                return false;
            }
            return true;

        case 'F':
            if (!normalize_numeric_value(candidate, flen, fdec, normalized)) {
                err_out = "invalid float for field";
                return false;
            }
            return true;

        default:
            return true;
    }
}

struct RecordLockGuard {
    xbase::DbArea& A;
    uint32_t rn = 0;
    bool locked = false;

    RecordLockGuard(xbase::DbArea& area, uint32_t recno, bool acquire) : A(area), rn(recno) {
        if (!acquire) return;
        std::string err;
        locked = xbase::locks::try_lock_record(A, rn, &err);
        if (!locked) throw std::runtime_error(err.empty() ? "lock failed" : err);
    }

    ~RecordLockGuard() {
        if (locked) xbase::locks::unlock_record(A, rn);
    }

    RecordLockGuard(const RecordLockGuard&) = delete;
    RecordLockGuard& operator=(const RecordLockGuard&) = delete;
};

} // namespace

void cmd_CALCWRITE(xbase::DbArea& area, std::istringstream& in) {
    if (!area.isOpen()) {
        std::cout << "CALCWRITE: no file open. Use: USE <table>\n";
        return;
    }

    std::string line;
    std::getline(in, line);
    line = trim(line);

    if (line.empty()) {
        std::cout << "Usage:\n  CALCWRITE <field> = <expr>\n";
        return;
    }

    std::string lhs, rhs;
    if (!parse_assignment(line, lhs, rhs)) {
        std::cout << "Usage:\n  CALCWRITE <field> = <expr>\n";
        return;
    }

    const int field1 = field_index_ci(area, lhs);
    if (field1 <= 0) {
        std::cout << "CALCWRITE: unknown field '" << lhs << "'\n";
        return;
    }

    const uint32_t rn = area.recno();
    if (rn == 0) {
        std::cout << "CALCWRITE: no current record.\n";
        return;
    }

    const int area0 = resolve_current_index(area);
    if (area0 < 0) {
        std::cout << "CALCWRITE: cannot determine current area.\n";
        return;
    }

    const auto& f = area.fields()[(size_t)(field1 - 1)];
    const char ftype = (char)std::toupper((unsigned char)f.type);
    const int  flen  = (int)f.length;
    const int  fdec  = (int)f.decimals;

    std::string eval_err;
    const dottalk::expr::EvalValue ev = dottalk::expr::eval_rhs(&area, rhs, &eval_err);
    if (ev.kind == dottalk::expr::EvalValue::K_None) {
        std::cout << "CALCWRITE error: " << (eval_err.empty() ? "evaluation failed" : eval_err) << "\n";
        return;
    }

    EvalResult er;
    switch (ev.kind) {
        case dottalk::expr::EvalValue::K_Number:
            er.kind = EvalResult::K_Number;
            er.number = ev.number;
            break;

        case dottalk::expr::EvalValue::K_String:
            er.kind = EvalResult::K_String;
            er.text = ev.text;
            break;

        case dottalk::expr::EvalValue::K_Bool:
            er.kind = EvalResult::K_Bool;
            er.tf = ev.tf;
            break;

        case dottalk::expr::EvalValue::K_Date:
            er.kind = EvalResult::K_Number;
            er.number = static_cast<double>(ev.date8);
            break;

        default:
            er.kind = EvalResult::K_None;
            break;
    }

    const std::string candidate_value = build_candidate_for_field(ftype, flen, fdec, er);

    std::string user_value = candidate_value;
    std::string normalize_err;
    if (!normalize_calcwrite_value_for_field(ftype, flen, fdec, candidate_value, user_value, normalize_err)) {
        std::cout << "CALCWRITE: " << normalize_err << ".\n";
        return;
    }

    std::string currency_norm;
    std::string currency_err;
    if (!cli_currency::validate_and_normalize_currency_pair_field(area, field1, user_value, currency_norm, currency_err)) {
        std::cout << "CALCWRITE: " << currency_err << ".\n";
        return;
    }
    user_value = currency_norm;

    std::string to_store = user_value;
    std::string memo_err;
    if (!build_x64_memo_stored_value(area, field1, user_value, to_store, memo_err)) {
        std::cout << "CALCWRITE: " << memo_err << ".\n";
        return;
    }

    std::string validate_err;
    if (!validate_field_value_for_store(area, field1, to_store, validate_err)) {
        std::cout << "CALCWRITE: " << validate_err << ".\n";
        return;
    }

    if (dottalk::table::is_enabled(area0)) {
        auto& tb = dottalk::table::get_tb(area0);

        std::uint64_t field_mask[dottalk::table::kWords]{};
        const int fldIndex0 = field1 - 1;
        const int word = fldIndex0 / 64;
        const int bit  = fldIndex0 % 64;
        if (word >= 0 && word < dottalk::table::kWords) field_mask[word] |= (std::uint64_t{1} << bit);

        tb.add_change((int)rn, dottalk::table::CHANGE_UPDATE, field_mask, field1, to_store);

        if (!dottalk::table::is_dirty(area0)) dottalk::table::set_dirty(area0, true);

        dottalk::table::mark_stale_field(area0, field1);

        if (Settings::instance().talk_on.load())
            std::cout << "CALCWRITE: buffered " << lhs << " = " << user_value << " at rec " << rn << ".\n";
        else
            std::cout << "CALCWRITE: buffered " << lhs << ".\n";
        return;
    }

    std::string before;
    std::string after;

    try {
        RecordLockGuard lock(area, rn, /*acquire=*/true);

        try { before = area.get(field1); } catch (...) { before.clear(); }

        bool ok = false;
        try {
            ok = area.set(field1, to_store) && area.writeCurrent();
        } catch (...) {
            ok = false;
        }

        if (!ok) {
            std::cout << "CALCWRITE: write failed.\n";
            return;
        }

        try { after = area.get(field1); } catch (...) { after.clear(); }

        if (before != after) dottalk::table::mark_stale_field(area0, field1);

        std::cout << "CALCWRITE: wrote " << lhs << " = " << user_value << "\n";
    } catch (const std::exception& e) {
        std::cout << "CALCWRITE: record is locked (" << e.what() << ").\n";
    } catch (...) {
        std::cout << "CALCWRITE: record is locked.\n";
    }
}