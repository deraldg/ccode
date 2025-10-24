#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <algorithm>
#include <limits>

#include "predicates.hpp"
#include "textio.hpp"

namespace {

// case-insensitive find
bool contains_ci(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    auto up = [](std::string s){
        for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    };
    std::string H = up(hay), N = up(needle);
    return H.find(N) != std::string::npos;
}

// trim both ends, leave internal spaces alone
inline std::string trim_both(std::string s) {
    return textio::trim(std::move(s));
}

inline bool parse_number(const std::string& s, double& out) {
    char* end = nullptr;
    const char* cs = s.c_str();
    out = std::strtod(cs, &end);
    if (end == cs) return false; // no parse
    // skip trailing spaces
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return *end == '\0';
}

} // namespace

namespace predicates {

// Return 1-based field index; 0 if not found (case-insensitive)
int field_index_ci(const xbase::DbArea& a, const std::string& name) {
    const auto& f = a.fields();
    for (size_t i = 0; i < f.size(); ++i) {
        if (textio::ieq(f[i].name, name)) return static_cast<int>(i + 1);
    }
    return 0;
}

// Evaluate simple predicate: <field> <op> <value>
// Supported ops:
//   =, ==           : equal (string CI or numeric if both parse)
//   !=, <>          : not equal
//   >, <, >=, <=    : numeric if both parse; else lexicographic CI
//   $               : substring contains (case-insensitive)
// Notes:
//   - Field value is read from the *current record* in DbArea.
//   - Caller should have positioned the record and (usually) unquoted the value already.
bool eval(const xbase::DbArea& a,
          const std::string& fld,
          const std::string& op,
          const std::string& val_in)
{
    int idx = field_index_ci(a, fld);
    if (idx <= 0) return false;

    // Fetch current record's field value
    std::string lhs_raw = a.get(idx);
    std::string rhs_raw = val_in;

    // Trim both sides
    std::string lhs = trim_both(lhs_raw);
    std::string rhs = trim_both(rhs_raw);

    // Try numeric compare when sensible
    double ln = 0.0, rn = 0.0;
    bool lhs_num = parse_number(lhs, ln);
    bool rhs_num = parse_number(rhs, rn);

    auto eq_ci = [](const std::string& a, const std::string& b){
        return textio::ieq(a, b);
    };
    auto lt_ci = [](const std::string& a, const std::string& b){
        // case-insensitive lexicographic compare
        size_t na = a.size(), nb = b.size();
        size_t n = std::min(na, nb);
        for (size_t i = 0; i < n; ++i) {
            unsigned char ca = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(a[i])));
            unsigned char cb = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(b[i])));
            if (ca < cb) return true;
            if (ca > cb) return false;
        }
        return na < nb;
    };

    // Normalize op to simple canonical forms
    std::string OP;
    OP.reserve(op.size());
    for (char c : op) OP.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));

    if (OP == "$" || OP == "CONTAINS") {
        return contains_ci(lhs, rhs);
    }

    if (OP == "=" || OP == "==") {
        if (lhs_num && rhs_num) return ln == rn;
        return eq_ci(lhs, rhs);
    }
    if (OP == "!=" || OP == "<>") {
        if (lhs_num && rhs_num) return ln != rn;
        return !eq_ci(lhs, rhs);
    }
    if (OP == ">") {
        if (lhs_num && rhs_num) return ln > rn;
        return !eq_ci(lhs, rhs) && !lt_ci(lhs, rhs);
    }
    if (OP == "<") {
        if (lhs_num && rhs_num) return ln < rn;
        return lt_ci(lhs, rhs);
    }
    if (OP == ">=") {
        if (lhs_num && rhs_num) return ln >= rn;
        return eq_ci(lhs, rhs) || (!lt_ci(lhs, rhs));
    }
    if (OP == "<=") {
        if (lhs_num && rhs_num) return ln <= rn;
        return eq_ci(lhs, rhs) || lt_ci(lhs, rhs);
    }

    // Unknown op -> false
    return false;
}

} // namespace predicates
