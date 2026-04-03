#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace xbase { class DbArea; }

namespace dottalk::expr {

// Forward declarations from your existing expr headers
struct Expr;
struct RecordView;

struct EvalValue {
    enum Kind { K_None, K_Number, K_String, K_Bool, K_Date } kind = K_None;

    double      number = 0.0;
    std::string text;
    bool        tf = false;
    std::int32_t date8 = 0;   // YYYYMMDD
};

// Evaluate a compiled program (no parsing/compiling here)
EvalValue eval_compiled_program(const Expr* prog, const RecordView& rv);

// Compile a WHERE-style expression to an AST program (moves out the program)
bool compile_where_program(const std::string& exprText,
                           std::unique_ptr<Expr>& programOut,
                           std::string* errOut = nullptr);

// Evaluate a “string value-expr” subset (field refs + string builtins + literals)
// This is the same fallback behavior CALC uses when the generic compiler can’t handle it.
bool eval_string_value_expr(xbase::DbArea& A,
                            const std::string& exprText,
                            std::string& out);

// Full pipeline (mirrors CALC):
// 1) predicate_chain fast path (boolean chains)
// 2) compile_where(...) + eval_compiled_program(...)
 // 3) eval_string_value_expr(...) fallback (string builtins/field refs)
// Returns typed value (bool/number/string/date) or K_None on failure.
EvalValue eval_any(xbase::DbArea& A,
                   const std::string& exprText,
                   std::string* errOut = nullptr);

// Boolean helper (FOR/WHILE):
// Accepts boolean results; also treats numeric as truthy (non-zero).
bool eval_bool(xbase::DbArea& A,
               const std::string& exprText,
               bool& out,
               std::string* errOut = nullptr);

} // namespace dottalk::expr
