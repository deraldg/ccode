// reserved_words.hpp
// Modern C++ representation of DotTalk++ reserved keywords (2026 dialect)
// For lexer/parser, syntax highlighting, autocompletion, etc.

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <algorithm>
#include <cctype>

namespace dottalkpp {

// All keywords in uppercase (case-insensitive matching is common in xBase dialects)
inline constexpr std::string_view keywords[] = {
    // Core commands
    "APPEND", "APPEND BLANK", "AREA", "AVERAGE", "AVG",
    "BROWSE", "CALC", "CALCWRITE", "CLEAR", "CLOSE",
    "COMMIT", "CONTINUE", "COPY", "COUNT", "CREATE",
    "DELETE", "DISPLAY", "DO", "DOTSCRIPT", "EDIT",
    "ELSE", "ENDIF", "ENDLOOP", "ENDSCAN", "ENDWHILE",
    "ENDUNTIL", "EXPORT", "FIND", "GO", "GOTO",
    "HELP", "IF", "IMPORT", "INDEX", "LIST",
    "LOCATE", "LOOP", "PACK", "RECALL", "REINDEX",
    "REL", "RELATIONS", "REPLACE", "ROLLBACK", "RUN",
    "SCAN", "SEEK", "SELECT", "SET", "SKIP",
    "SORT", "TABLE", "TEST", "TOP", "TUPLE",
    "TUP", "TUPTALK", "UNTIL", "UPDATE", "USE",
    "VALIDATE", "WHILE", "WSREPORT", "ZAP",

    // Common clauses/modifiers
    "ALL", "DELETED", "FOR", "NEXT", "REST",
    "ON", "TO", "WITH", "UNIQUE", "OVERWRITE",
    "ASC", "DESC", "PHYSICAL", "NOINDEX", "NOIDX",

    // Schema / Relations
    "SCHEMAS", "SCHEMA",
    "REL", "RELATIONS", "RELATION",
    "ADD", "CLEAR", "LIST", "REFRESH", "SAVE",
    "LOAD", "JOIN", "ENUM", "ONE", "DISTINCT",
    "LIMIT",

    // TABLE family
    "TABLE", "ON", "OFF", "STALE", "FRESH", "DIRTY",

    // SET family
    "SET", "SETPATH", "SETORDER", "SETINDEX", "SETCNX",
    "SETFILTER", "SET RELATION", "SET UNIQUE", "SETCASE",

    // Functions / expression words
    "DATE", "TODAY", "DATETIME", "NOW", "TIME",
    "DATEADD", "DATEDIFF", "CTOD", "DTOC",
    "UPPER", "LOWER", "PROPER", "ALLTRIM", "TRIM",
    "LTRIM", "RTRIM", "LEFT", "RIGHT", "SUBSTR",
    "STUFF", "SPACE", "REPLICATE", "PADR", "PADL",
    "PADC", "STR", "VAL", "CHR", "ASC",
    "ATC", "AT", "LEN", "LIKE", "CONCAT", "STRCAT",
};

// Helper: case-insensitive lookup
[[nodiscard]] inline bool is_reserved(std::string_view word) noexcept {
    if (word.empty()) return false;

    // Fast path: most words are short → avoid allocation when possible
    std::string upper(word.size(), '\0');
    std::transform(word.begin(), word.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    static const std::unordered_set<std::string_view> reserved_set{
        std::begin(keywords), std::end(keywords)
    };

    return reserved_set.contains(upper);
}

// Convenience overload for std::string
[[nodiscard]] inline bool is_reserved(const std::string& s) noexcept {
    return is_reserved(std::string_view{s});
}

} // namespace dottalkpp