// src/cli/evaluate.cpp
// Phase 1: AST-backed predicate compilation (replaces the legacy mini-predicate
// compiler). This aligns EVALUATE/COUNT/LOCATE/etc. with the same expression
// grammar used by the "WHERE" engine.

#include "cli/expr/evaluate.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>

#include "cli/expr/api.hpp"        // compile_where
#include "cli/expr/glue_xbase.hpp" // make_record_view

namespace dottalk::expr {

namespace {

static inline bool is_space(char c) {
  return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static std::string trim_copy(std::string_view sv) {
  size_t b = 0;
  size_t e = sv.size();
  while (b < e && is_space(sv[b])) ++b;
  while (e > b && is_space(sv[e - 1])) --e;
  return std::string(sv.substr(b, e - b));
}

static bool is_quoted(const std::string& s) {
  return s.size() >= 2 &&
         ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''));
}

// Minimal compatibility normalization:
// - Allow users to pass a quoted predicate string.
// - Preserve legacy DELETED() usage by rewriting it to DELETED.
static std::string normalize_predicate_text(std::string_view text) {
  std::string s = trim_copy(text);
  if (is_quoted(s)) {
    s = trim_copy(std::string_view(s).substr(1, s.size() - 2));
  }

  // Rewrite DELETED() -> DELETED (case-insensitive, optional spaces).
  // We only handle the exact token DELETED with empty parens.
  auto ieq = [](char a, char b) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(a))) ==
           static_cast<char>(std::toupper(static_cast<unsigned char>(b)));
  };

  auto starts_with_i = [&](const std::string& hay, std::string_view needle) {
    const size_t n = needle.size();
    if (hay.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
      if (!ieq(hay[i], needle[i])) return false;
    }
    return true;
  };

  std::string t = trim_copy(s);
  if (starts_with_i(t, std::string_view{"DELETED"})) {
    size_t i = 7; // len("DELETED")
    while (i < t.size() && is_space(t[i])) ++i;
    if (i < t.size() && t[i] == '(') {
      ++i;
      while (i < t.size() && is_space(t[i])) ++i;
      if (i < t.size() && t[i] == ')') {
        ++i;
        while (i < t.size() && is_space(t[i])) ++i;
        if (i == t.size()) {
          return "DELETED";
        }
      }
    }
  }

  return s;
}

struct LitPredImpl {
  bool value{false};
};

static bool lit_eval(void* impl, const xbase::DbArea&) {
  const auto* p = static_cast<const LitPredImpl*>(impl);
  return p ? p->value : false;
}

static void lit_destroy(void* impl) {
  delete static_cast<LitPredImpl*>(impl);
}

struct AstPredImpl {
  std::unique_ptr<Expr> program;
};

static bool ast_eval(void* impl, const xbase::DbArea& area) {
  const auto* p = static_cast<const AstPredImpl*>(impl);
  if (!p || !p->program) return false;
  auto rv = glue::make_record_view(const_cast<xbase::DbArea&>(area));
  return p->program->eval(rv);
}

static void ast_destroy(void* impl) {
  delete static_cast<AstPredImpl*>(impl);
}

} // namespace

CompileOut compile_literal(bool value) {
  CompileOut out;
  out.pred.impl = new LitPredImpl{value};
  out.pred.eval = &lit_eval;
  out.pred.destroy = &lit_destroy;
  return out;
}

CompileOut compile_predicate(std::string expr_text, const xbase::DbArea& area) {
  (void)area; // reserved (future: schema validation)

  CompileOut out;
  const std::string normalized = normalize_predicate_text(expr_text);
  if (trim_copy(normalized).empty()) {
    return compile_literal(true);
  }

  // Compile with the AST "WHERE" grammar.
  CompileResult cr = compile_where(normalized);
  if (!cr) {
    out.error = cr.error.empty() ? "predicate parse error" : cr.error;
    return out;
  }

  auto* impl = new AstPredImpl{};
  impl->program = std::move(cr.program);
  out.pred.impl = impl;
  out.pred.eval = &ast_eval;
  out.pred.destroy = &ast_destroy;
  return out;
}

} // namespace dottalk::expr
