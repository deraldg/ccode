#include "shell_lexicon.hpp"

#include "textio.hpp"

#include <array>
#include <string>
#include <string_view>

namespace dottalk::lexicon {
namespace {

struct Entry {
    const char* token;
    TokenInfo info;
};

constexpr TokenInfo kUnknown{
    TokenClass::Unknown,
    ReservedStrength::None,
    true,
    nullptr
};

constexpr std::array<Entry, 50> kEntries{{
    // Hard control / block vocabulary
    {"IF",        {TokenClass::ControlOpen,  ReservedStrength::Hard, false, "IF"}},
    {"ELSE",      {TokenClass::ControlMid,   ReservedStrength::Hard, false, "ELSE"}},
    {"ENDIF",     {TokenClass::ControlClose, ReservedStrength::Hard, false, "ENDIF"}},

    {"LOOP",      {TokenClass::ControlOpen,  ReservedStrength::Hard, false, "LOOP"}},
    {"ENDLOOP",   {TokenClass::ControlClose, ReservedStrength::Hard, false, "ENDLOOP"}},

    {"WHILE",     {TokenClass::ControlOpen,  ReservedStrength::Hard, false, "WHILE"}},
    {"ENDWHILE",  {TokenClass::ControlClose, ReservedStrength::Hard, false, "ENDWHILE"}},

    {"UNTIL",     {TokenClass::ControlOpen,  ReservedStrength::Hard, false, "UNTIL"}},
    {"ENDUNTIL",  {TokenClass::ControlClose, ReservedStrength::Hard, false, "ENDUNTIL"}},

    {"SCAN",      {TokenClass::ControlOpen,  ReservedStrength::Hard, false, "SCAN"}},
    {"ENDSCAN",   {TokenClass::ControlClose, ReservedStrength::Hard, false, "ENDSCAN"}},

    {"CONTINUE",  {TokenClass::ControlMid,   ReservedStrength::Hard, false, "CONTINUE"}},

    // Expression / predicate operators
    {"AND",       {TokenClass::ExprOperator, ReservedStrength::Hard, false, "AND"}},
    {"OR",        {TokenClass::ExprOperator, ReservedStrength::Hard, false, "OR"}},
    {"NOT",       {TokenClass::ExprOperator, ReservedStrength::Hard, false, "NOT"}},
    {"LIKE",      {TokenClass::ExprOperator, ReservedStrength::Hard, false, "LIKE"}},

    // Soft qualifiers / grammar words
    {"ALL",       {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "ALL"}},
    {"FOR",       {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "FOR"}},
    {"IN",        {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "IN"}},
    {"TO",        {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "TO"}},
    {"TAG",       {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "TAG"}},
    {"ON",        {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "ON"}},
    {"OFF",       {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "OFF"}},
    {"LIMIT",     {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "LIMIT"}},
    {"INTO",      {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "INTO"}},
    {"FIELDS",    {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "FIELDS"}},
    {"PHYSICAL",  {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "PHYSICAL"}},
    {"ADDITIVE",  {TokenClass::SoftQualifier, ReservedStrength::Soft, false, "ADDITIVE"}},

    // Command heads (reserved for DotScript variable-name purposes)
    {"SET",       {TokenClass::CommandHead, ReservedStrength::Hard, false, "SET"}},
    {"USE",       {TokenClass::CommandHead, ReservedStrength::Hard, false, "USE"}},
    {"SELECT",    {TokenClass::CommandHead, ReservedStrength::Hard, false, "SELECT"}},
    {"LIST",      {TokenClass::CommandHead, ReservedStrength::Hard, false, "LIST"}},
    {"DISPLAY",   {TokenClass::CommandHead, ReservedStrength::Hard, false, "DISPLAY"}},
    {"COUNT",     {TokenClass::CommandHead, ReservedStrength::Hard, false, "COUNT"}},
    {"SEEK",      {TokenClass::CommandHead, ReservedStrength::Hard, false, "SEEK"}},
    {"FIND",      {TokenClass::CommandHead, ReservedStrength::Hard, false, "FIND"}},
    {"LOCATE",    {TokenClass::CommandHead, ReservedStrength::Hard, false, "LOCATE"}},
    {"REPLACE",   {TokenClass::CommandHead, ReservedStrength::Hard, false, "REPLACE"}},
    {"REL",       {TokenClass::CommandHead, ReservedStrength::Hard, false, "REL"}},
    {"SCHEMAS",   {TokenClass::CommandHead, ReservedStrength::Hard, false, "SCHEMAS"}},
    {"STRUCT",    {TokenClass::CommandHead, ReservedStrength::Hard, false, "STRUCT"}},
    {"TUPLE",     {TokenClass::CommandHead, ReservedStrength::Hard, false, "TUPLE"}},
    {"SHOW",      {TokenClass::CommandHead, ReservedStrength::Hard, false, "SHOW"}},
    {"STATUS",    {TokenClass::CommandHead, ReservedStrength::Hard, false, "STATUS"}},
    {"GO",        {TokenClass::CommandHead, ReservedStrength::Hard, false, "GO"}},
    {"CALC",      {TokenClass::CommandHead, ReservedStrength::Hard, false, "CALC"}},
    {"CALCWRITE", {TokenClass::CommandHead, ReservedStrength::Hard, false, "CALCWRITE"}},
    {"DOTSCRIPT", {TokenClass::CommandHead, ReservedStrength::Hard, false, "DOTSCRIPT"}},
    {"TEST",      {TokenClass::CommandHead, ReservedStrength::Hard, false, "TEST"}},
    {"VAR",       {TokenClass::CommandHead, ReservedStrength::Hard, false, "VAR"}}
}};

} // namespace

std::string normalize_token(std::string_view tok)
{
    return textio::up(textio::trim(std::string(tok)));
}

const TokenInfo& classify_token(std::string_view tok)
{
    static const TokenInfo kUnknownStatic = kUnknown;
    const std::string u = normalize_token(tok);
    if (u.empty()) return kUnknownStatic;

    for (const auto& e : kEntries) {
        if (u == e.token) return e.info;
    }
    return kUnknownStatic;
}

bool is_hard_control(std::string_view tok)
{
    const TokenInfo& info = classify_token(tok);
    return (info.reserved == ReservedStrength::Hard) &&
           (info.cls == TokenClass::ControlOpen ||
            info.cls == TokenClass::ControlMid ||
            info.cls == TokenClass::ControlClose);
}

bool is_expr_reserved(std::string_view tok)
{
    return classify_token(tok).cls == TokenClass::ExprOperator;
}

bool is_soft_qualifier(std::string_view tok)
{
    return classify_token(tok).cls == TokenClass::SoftQualifier;
}

bool is_command_head(std::string_view tok)
{
    return classify_token(tok).cls == TokenClass::CommandHead;
}

bool is_valid_var_name_token(std::string_view tok)
{
    return classify_token(tok).valid_var_name;
}

bool is_if_family_token(std::string_view tok)
{
    const std::string u = normalize_token(tok);
    return u == "IF" || u == "ELSE" || u == "ENDIF";
}

bool is_loop_family_token(std::string_view tok)
{
    const std::string u = normalize_token(tok);
    return u == "LOOP" || u == "ENDLOOP" ||
           u == "WHILE" || u == "ENDWHILE" ||
           u == "UNTIL" || u == "ENDUNTIL" ||
           u == "SCAN" || u == "ENDSCAN" ||
           u == "CONTINUE";
}

} // namespace dottalk::lexicon
