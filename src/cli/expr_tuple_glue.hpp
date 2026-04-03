#pragma once
// Bind the dottalk::expr AST engine to TupleRow for tuple-value FOR evaluation.
// Built-ins: RECNO(), DELETED(), best-effort EMPTY(<FIELD>).
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <cctype>
#include <algorithm>

#include "tuple_types.hpp"       // TupleRow, columns/values/fragments
#include "cli/expr/eval.hpp"         // dottalk::expr::to_number, iequals
#include "cli/expr/ast.hpp"          // dottalk::expr::RecordView

namespace dottalk::exprglue {

enum class Collation : uint8_t { CS = 0, CI = 1 };
inline Collation g_collation = Collation::CS;
inline void set_collation_ci(bool on) { g_collation = on ? Collation::CI : Collation::CS; }
inline bool is_ci() { return g_collation == Collation::CI; }
inline std::string norm_by_collation(std::string s) {
    if (!is_ci()) return s;
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

inline std::string g_last_ambiguity;
inline bool g_has_ambiguity = false;
inline void note_ambiguity(const std::string& short_name_up) {
    if (!g_has_ambiguity) { g_has_ambiguity = true; g_last_ambiguity = short_name_up; }
}
inline std::optional<std::string> pop_ambiguity_hint() {
    if (!g_has_ambiguity) return std::nullopt;
    g_has_ambiguity = false;
    return g_last_ambiguity;
}


struct CiIndex {
    std::unordered_map<std::string, size_t> by_up;
    std::unordered_map<std::string, size_t> by_short; // suffix map for short names
    static std::string up(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }
    static std::string short_name(const std::string& full) {
        auto pos = full.rfind('.');
        return (pos == std::string::npos) ? full : full.substr(pos + 1);
    }
    explicit CiIndex(const TupleRow& row) {
        for (size_t i = 0; i < row.columns.size(); ++i) {
            const auto& full = row.columns[i].name;
            const auto upfull = up(full);
            by_up.emplace(upfull, i);
            const auto sh = up(short_name(full));
            if (!by_short.count(sh)) by_short.emplace(sh, i);
            else by_short[sh] = static_cast<size_t>(-1);
        }
    }
    std::optional<size_t> find(std::string_view name) const {
        const std::string upname = up(std::string(name));
        auto it = by_up.find(upname);
        if (it != by_up.end()) return it->second;
        auto jt = by_short.find(upname);
        if (jt != by_short.end()) {
            if (jt->second != static_cast<size_t>(-1)) return jt->second;
            note_ambiguity(upname);
        }
        return std::nullopt;
    }
};

inline std::optional<double> tuple_recno_number(const TupleRow& row) {
    if (row.fragments.empty()) return std::nullopt;
    return static_cast<double>(row.fragments.front().recno);
}

inline std::string trim(std::string s) {
    auto issp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}

// Best-effort parser for EMPTY(<FIELD>) when the engine treats it as an identifier.
inline std::optional<bool> try_eval_empty_identifier(const std::string& ident, const CiIndex& idx, const TupleRow& row) {
    auto up = [](std::string s){ for (auto& c: s) c=(char)std::toupper((unsigned char)c); return s; };
    std::string U = up(ident);
    if (U.rfind("EMPTY(", 0) != 0) return std::nullopt;
    if (U.back() != ')') return std::nullopt;
    std::string inner = trim(std::string(ident.begin()+6, ident.end()-1));
    if (inner.empty()) return std::optional<bool>(true);
    if (auto pos = idx.find(inner)) {
        const std::string& v = row.values[*pos];
        bool only_space = std::all_of(v.begin(), v.end(), [](unsigned char c){ return std::isspace(c); });
        return std::optional<bool>(only_space || v.empty());
    }
    return std::optional<bool>(true);
}

inline dottalk::expr::RecordView make_record_view(const TupleRow& row) {
    CiIndex idx(row);
    dottalk::expr::RecordView rv;

    rv.get_field_str = [row, idx](std::string_view name) -> std::string {
        if (dottalk::expr::iequals(name, "RECNO()") || dottalk::expr::iequals(name, "RECNO")) {
            if (auto n = tuple_recno_number(row)) {
                long v = static_cast<long>(*n + 1e-9);
                return std::to_string(v);
            }
            return {};
        }
        if (dottalk::expr::iequals(name, "DELETED()") || dottalk::expr::iequals(name, "DELETED")) {
            return "0";
        }
        if (auto em = try_eval_empty_identifier(std::string(name), idx, row)) {
            return (*em ? "1" : "0");
        }
        if (auto pos = idx.find(name)) return norm_by_collation(row.values[*pos]);
        auto is_bare = [](std::string_view s){
            if (s.empty()) return false;
            for (unsigned char c : s) {
                if (!(std::isalpha(c) || c=='_' )) return false;
            }
            return true;
        };
        if (is_bare(name)) return norm_by_collation(std::string(name)); // treat as literal string
        return {};
    };

    rv.get_field_num = [row, idx](std::string_view name) -> std::optional<double> {
        if (dottalk::expr::iequals(name, "RECNO()") || dottalk::expr::iequals(name, "RECNO")) {
            return tuple_recno_number(row);
        }
        if (dottalk::expr::iequals(name, "DELETED()") || dottalk::expr::iequals(name, "DELETED")) {
            return 0.0;
        }
        if (auto em = try_eval_empty_identifier(std::string(name), idx, row)) {
            return (*em ? 1.0 : 0.0);
        }
        if (auto pos = idx.find(name)) return dottalk::expr::to_number(row.values[*pos]);
        return std::nullopt;
    };

    return rv;
}

} // namespace dottalk::exprglue
