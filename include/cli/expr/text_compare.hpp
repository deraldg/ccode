#pragma once

#include <string>

namespace dottalk::expr {

enum class TextMatchKind {
    None,
    FoldedOnly,
    Exact
};

TextMatchKind compare_text_values(const std::string& lhs,
                                  const std::string& rhs);

bool text_match_is_true(TextMatchKind m, bool case_on);

std::string normalize_text_exact(const std::string& s);
std::string normalize_text_folded(const std::string& s);

} // namespace dottalk::expr
