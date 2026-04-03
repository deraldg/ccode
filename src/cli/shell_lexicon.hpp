#pragma once

#include <string>
#include <string_view>

namespace dottalk::lexicon {

enum class TokenClass {
    Unknown,
    ControlOpen,
    ControlClose,
    ControlMid,
    ExprOperator,
    SoftQualifier,
    CommandHead,
    AliasOnly
};

enum class ReservedStrength {
    None,
    Soft,
    Hard
};

struct TokenInfo {
    TokenClass cls = TokenClass::Unknown;
    ReservedStrength reserved = ReservedStrength::None;
    bool valid_var_name = true;
    const char* canonical = nullptr;
};

// Normalize a token to canonical comparison form (trim + uppercase).
std::string normalize_token(std::string_view tok);

// Classify one bare token.
// Notes:
//   - Classification is exact-match and case-insensitive.
//   - This layer does not parse whole commands; it only classifies one token.
const TokenInfo& classify_token(std::string_view tok);

// Helper predicates
bool is_hard_control(std::string_view tok);
bool is_expr_reserved(std::string_view tok);
bool is_soft_qualifier(std::string_view tok);
bool is_command_head(std::string_view tok);
bool is_valid_var_name_token(std::string_view tok);

// Optional convenience helpers for shell/control code
bool is_if_family_token(std::string_view tok);
bool is_loop_family_token(std::string_view tok);
bool is_block_open_token(std::string_view tok);
bool is_block_close_token(std::string_view tok);

} // namespace dottalk::lexicon
