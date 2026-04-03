#include "cli/expr/text_compare.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace dottalk::expr {
namespace {

inline std::string trim_both_copy(std::string s) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };

    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [&](unsigned char c){ return !is_space(c); }));

    s.erase(std::find_if(s.rbegin(), s.rend(),
        [&](unsigned char c){ return !is_space(c); }).base(), s.end());

    return s;
}

inline std::string upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

} // namespace

std::string normalize_text_exact(const std::string& s) {
    return trim_both_copy(s);
}

std::string normalize_text_folded(const std::string& s) {
    return upper_copy(normalize_text_exact(s));
}

TextMatchKind compare_text_values(const std::string& lhs,
                                  const std::string& rhs)
{
    const std::string lhs_exact = normalize_text_exact(lhs);
    const std::string rhs_exact = normalize_text_exact(rhs);

    if (lhs_exact == rhs_exact) {
        return TextMatchKind::Exact;
    }

    const std::string lhs_fold = upper_copy(lhs_exact);
    const std::string rhs_fold = upper_copy(rhs_exact);

    if (lhs_fold == rhs_fold) {
        return TextMatchKind::FoldedOnly;
    }

    return TextMatchKind::None;
}

bool text_match_is_true(TextMatchKind m, bool case_on)
{
    if (case_on) {
        return m == TextMatchKind::Exact;
    }
    return m != TextMatchKind::None;
}

} // namespace dottalk::expr
