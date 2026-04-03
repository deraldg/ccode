// src/cli/shell_api_extras.cpp
#include "shell_api_extras.hpp"
#include <cctype>

namespace {

// returns true if 's' starts with token sequence, ignoring case and
// requiring token boundaries (space/Tab/EOL) after each token.
static bool starts_with_tokens_ci(const std::string& s,
                                  const std::string& t1,
                                  const std::string& t2,
                                  size_t* boundary_out /* end pos of matched prefix */)
{
    size_t i = 0;
    // skip leading whitespace
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;

    // match t1
    for (size_t j = 0; j < t1.size(); ++j) {
        if (i + j >= s.size()) return false;
        if (std::toupper((unsigned char)s[i + j]) != std::toupper((unsigned char)t1[j])) return false;
    }
    size_t p = i + t1.size();
    if (p < s.size() && s[p] != ' ' && s[p] != '\t') return false; // need boundary

    // skip ws
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) ++p;

    // match t2
    for (size_t j = 0; j < t2.size(); ++j) {
        if (p + j >= s.size()) return false;
        if (std::toupper((unsigned char)s[p + j]) != std::toupper((unsigned char)t2[j])) return false;
    }
    size_t q = p + t2.size();
    if (q < s.size() && s[q] != ' ' && s[q] != '\t') return false;

    // skip ws to args start
    while (q < s.size() && (s[q] == ' ' || s[q] == '\t')) ++q;
    if (boundary_out) *boundary_out = q;
    return true;
}

static bool starts_with_token_ci(const std::string& s,
                                 const std::string& t1,
                                 size_t* boundary_out)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;

    for (size_t j = 0; j < t1.size(); ++j) {
        if (i + j >= s.size()) return false;
        if (std::toupper((unsigned char)s[i + j]) != std::toupper((unsigned char)t1[j])) return false;
    }
    size_t p = i + t1.size();
    if (p < s.size() && s[p] != ' ' && s[p] != '\t') return false;

    while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) ++p;
    if (boundary_out) *boundary_out = p;
    return true;
}

} // namespace

namespace cli {

std::string preprocess_for_dispatch(const std::string& line)
{
    // 1) SET RELATIONS <args...> -> REL <args...>
    size_t after_prefix = 0;
    if (starts_with_tokens_ci(line, "SET", "RELATIONS", &after_prefix)) {
        return "REL " + line.substr(after_prefix);
    }

    // 2) RELATIONS <args...> -> REL <args...>
    if (starts_with_token_ci(line, "RELATIONS", &after_prefix)) {
        return "REL " + line.substr(after_prefix);
    }

    // 3) pass-through
    return line;
}

} // namespace cli