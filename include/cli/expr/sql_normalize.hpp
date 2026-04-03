#pragma once
#include <string>

namespace sqlnorm {

// Convert a simple SQL-style boolean expression (case-insensitive) into a
// DotTalk-compatible WHERE string.
//
// Rules implemented:
// - Keywords AND/OR/NOT are always reserved (never identifiers)
// - AND  -> .AND.
// - OR   -> .OR.
// - NOT (value)           -> handled via boolean NOT or <> rewrites (see below)
// - IDENT NOT <value>     -> IDENT <> <value>
// - <> and != both become <>
// - Identifiers uppercased (field names)
// - Bare values (ident-like) auto-quoted; numbers are preserved; quoted strings kept
// - Parentheses preserved
//
// Examples:
//   lname = grimwood and fname not derald
//     -> LNAME = "GRIMWOOD" .AND. FNAME <> "DERALD"
//
//   not (gpa > 1)
//     -> .NOT. (GPA > 1)
//
//   lname <> grimwood
//     -> LNAME <> "GRIMWOOD"
//
//   lname = (grimwood and fname = derald) or (lname = grimwood and gpa > 1)
//     -> LNAME = ("GRIMWOOD" .AND. FNAME = "DERALD") .OR.
//        (LNAME = "GRIMWOOD" .AND. GPA > 1)
//
std::string sql_to_dottalk_where(const std::string& input);

} // namespace sqlnorm



