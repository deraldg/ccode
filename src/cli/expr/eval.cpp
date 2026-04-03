#include "cli/expr/ast.hpp"
#include "cli/expr/eval.hpp"
#include "cli/expr/text_compare.hpp"
#include "predicate_eval.hpp"

#include <cctype>
#include <optional>
#include <string>

using namespace dottalk::expr;

bool FieldRef::eval(const RecordView& rv) const {
  if (!rv.get_field_str) return false;
  auto s = rv.get_field_str(name);
  for (char ch: s) {
    if (!std::isspace(static_cast<unsigned char>(ch))) return true;
  }
  return false;
}

static std::string value_as_string(const RecordView& rv, const Expr* e) {
  if (auto fr = dynamic_cast<const FieldRef*>(e)) {
    if (rv.get_field_str) return rv.get_field_str(fr->name);
    return {};
  }
  if (auto ls = dynamic_cast<const LitString*>(e)) return ls->v;
  if (auto ln = dynamic_cast<const LitNumber*>(e)) return std::to_string(ln->v);
  return e->eval(rv) ? "1" : "0";
}

static std::optional<double> value_as_number(const RecordView& rv, const Expr* e) {
  if (auto ln = dynamic_cast<const LitNumber*>(e)) return ln->v;
  if (auto fr = dynamic_cast<const FieldRef*>(e)) {
    if (rv.get_field_num) return rv.get_field_num(fr->name);
    if (rv.get_field_str) return to_number(rv.get_field_str(fr->name));
    return std::nullopt;
  }
  if (auto ls = dynamic_cast<const LitString*>(e)) return to_number(ls->v);
  if (auto ar = dynamic_cast<const Arith*>(e))  return ar->evalNumber(rv);
  return std::nullopt;
}

bool Cmp::eval(const RecordView& rv) const {
  auto ln = value_as_number(rv, lhs.get());
  auto rn = value_as_number(rv, rhs.get());
  if (ln && rn) {
    double a = *ln, b = *rn;
    switch (op) {
      case CmpOp::EQ: return a == b;
      case CmpOp::NE: return a != b;
      case CmpOp::LT: return a <  b;
      case CmpOp::LE: return a <= b;
      case CmpOp::GT: return a >  b;
      case CmpOp::GE: return a >= b;
    }
  }

  std::string as = value_as_string(rv, lhs.get());
  std::string bs = value_as_string(rv, rhs.get());

  const bool case_on = predx::get_case_sensitive();
  const auto match = dottalk::expr::compare_text_values(as, bs);

  switch (op) {
    case CmpOp::EQ:
      return dottalk::expr::text_match_is_true(match, case_on);

    case CmpOp::NE:
      return !dottalk::expr::text_match_is_true(match, case_on);

    case CmpOp::LT:
    case CmpOp::LE:
    case CmpOp::GT:
    case CmpOp::GE: {
      const std::string lhs_exact = dottalk::expr::normalize_text_exact(as);
      const std::string rhs_exact = dottalk::expr::normalize_text_exact(bs);

      if (case_on) {
        if (op == CmpOp::LT) return lhs_exact <  rhs_exact;
        if (op == CmpOp::LE) return lhs_exact <= rhs_exact;
        if (op == CmpOp::GT) return lhs_exact >  rhs_exact;
        if (op == CmpOp::GE) return lhs_exact >= rhs_exact;
      } else {
        const std::string lhs_fold = dottalk::expr::normalize_text_folded(as);
        const std::string rhs_fold = dottalk::expr::normalize_text_folded(bs);

        if (op == CmpOp::LT) return lhs_fold <  rhs_fold;
        if (op == CmpOp::LE) return lhs_fold <= rhs_fold;
        if (op == CmpOp::GT) return lhs_fold >  rhs_fold;
        if (op == CmpOp::GE) return lhs_fold >= rhs_fold;
      }
    }
  }

  return false;
}

bool BoolBin::eval(const RecordView& rv) const {
  if (op==BoolOp::AND) {
    if (!lhs->eval(rv)) return false;
    return rhs->eval(rv);
  } else {
    if (lhs->eval(rv)) return true;
    return rhs->eval(rv);
  }
}

bool Not::eval(const RecordView& rv) const { return !inner->eval(rv); }

double Arith::evalNumber(const RecordView& rv) const {
  auto L = value_as_number(rv, lhs.get()).value_or(0.0);
  auto R = value_as_number(rv, rhs.get()).value_or(0.0);
  switch (op) {
    case ArithOp::Add: return L + R;
    case ArithOp::Sub: return L - R;
    case ArithOp::Mul: return L * R;
    case ArithOp::Div: return (R==0.0 ? 0.0 : L / R);
  }
  return 0.0;
}

bool Arith::eval(const RecordView& rv) const {
  return evalNumber(rv) != 0.0;
}
