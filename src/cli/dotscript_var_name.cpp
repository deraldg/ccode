#include "dotscript_var_name.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_set>

namespace dottalk::dotscript {
namespace {

using Set = std::unordered_set<std::string>;

const Set& hard_reserved_set() {
    static const Set kSet = {
        "IF", "ELSE", "ENDIF",
        "DO", "WHILE", "ENDDO", "FOR", "TO", "STEP", "ENDFOR", "SCAN", "ENDSCAN",
        "CASE", "OTHERWISE", "ENDCASE", "RETURN",
        "VAR"
    };
    return kSet;
}

const Set& expression_reserved_set() {
    static const Set kSet = {
        "AND", "OR", "NOT",
        "IN", "LIKE", "BETWEEN",
        "TRUE", "FALSE", "NULL"
    };
    return kSet;
}

const Set& command_head_reserved_set() {
    static const Set kSet = {
        "SET", "USE", "SELECT", "LIST", "DISPLAY", "COUNT", "SEEK", "FIND", "LOCATE",
        "COPY", "DELETE", "RECALL", "REPLACE", "APPEND", "STRUCT", "BROWSE", "STATUS",
        "AREA", "SCHEMAS", "INDEX", "REINDEX", "ORDER", "BUILDLMDB", "SETLMDB"
    };
    return kSet;
}

const Set& soft_reserved_set() {
    static const Set kSet = {
        "ALL", "NEXT", "REST", "TAG", "FIELDS", "WITH", "FROM",
        "ON", "OFF", "YES", "NO"
    };
    return kSet;
}

bool is_identifier_start(unsigned char ch) {
    return std::isalpha(ch) != 0 || ch == '_';
}

bool is_identifier_continue(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_';
}

bool looks_quoted(std::string_view s) {
    return s.size() >= 2 && s.front() == '"' && s.back() == '"';
}

std::string make_message(std::string_view token, const char* why) {
    std::string msg = "Invalid variable name '";
    msg.append(token.begin(), token.end());
    msg += "': ";
    msg += why;
    return msg;
}

} // namespace

std::string uppercase_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s) {
        out.push_back(static_cast<char>(std::toupper(ch)));
    }
    return out;
}

bool is_hard_reserved(std::string_view token) {
    return hard_reserved_set().find(uppercase_ascii(token)) != hard_reserved_set().end();
}

bool is_expression_reserved(std::string_view token) {
    return expression_reserved_set().find(uppercase_ascii(token)) != expression_reserved_set().end();
}

bool is_command_head_reserved(std::string_view token) {
    return command_head_reserved_set().find(uppercase_ascii(token)) != command_head_reserved_set().end();
}

bool is_soft_reserved(std::string_view token) {
    return soft_reserved_set().find(uppercase_ascii(token)) != soft_reserved_set().end();
}

VarNameCheck validate_var_name(std::string_view token) {
    VarNameCheck out;
    out.normalized = uppercase_ascii(token);

    if (token.empty()) {
        out.issue = VarNameIssue::Empty;
        out.message = "Invalid variable name: empty identifier";
        return out;
    }

    if (looks_quoted(token)) {
        out.issue = VarNameIssue::QuotedIdentifierNotSupported;
        out.message = make_message(token, "quoted identifiers are not supported");
        return out;
    }

    if (!is_identifier_start(static_cast<unsigned char>(token.front()))) {
        out.issue = VarNameIssue::MustStartWithLetterOrUnderscore;
        out.message = make_message(token, "must start with a letter or underscore");
        return out;
    }

    for (std::size_t i = 1; i < token.size(); ++i) {
        if (!is_identifier_continue(static_cast<unsigned char>(token[i]))) {
            out.issue = VarNameIssue::ContainsInvalidCharacter;
            out.message = make_message(token, "may contain only letters, digits, and underscore");
            return out;
        }
    }

    if (hard_reserved_set().find(out.normalized) != hard_reserved_set().end()) {
        out.issue = VarNameIssue::ReservedControlWord;
        out.message = make_message(token, "reserved control word");
        return out;
    }

    if (expression_reserved_set().find(out.normalized) != expression_reserved_set().end()) {
        out.issue = VarNameIssue::ReservedExpressionWord;
        out.message = make_message(token, "reserved expression operator");
        return out;
    }

    if (command_head_reserved_set().find(out.normalized) != command_head_reserved_set().end()) {
        out.issue = VarNameIssue::ReservedCommandWord;
        out.message = make_message(token, "reserved command word");
        return out;
    }

    if (soft_reserved_set().find(out.normalized) != soft_reserved_set().end()) {
        out.issue = VarNameIssue::ReservedScopeQualifier;
        out.message = make_message(token, "reserved scope qualifier");
        return out;
    }

    out.ok = true;
    out.issue = VarNameIssue::None;
    out.message.clear();
    return out;
}

} // namespace dottalk::dotscript
