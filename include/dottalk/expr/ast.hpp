#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace dottalk { namespace expr {

struct RecordView {
  std::function<std::string(std::string_view)> get_field_str;
  std::function<std::optional<double>(std::string_view)> get_field_num;
};

struct Expr {
  virtual ~Expr() = default;
  virtual bool eval(const RecordView& rv) const = 0;
};

struct LitString : Expr {
  std::string v;
  explicit LitString(std::string s): v(std::move(s)) {}
  bool eval(const RecordView&) const override { return !v.empty(); }
};

struct LitNumber : Expr {
  double v;
  explicit LitNumber(double d): v(d) {}
  bool eval(const RecordView&) const override { return v != 0.0; }
};

struct FieldRef : Expr {
  std::string name;
  explicit FieldRef(std::string n): name(std::move(n)) {}
  bool eval(const RecordView& rv) const override;
};

enum class CmpOp { EQ, NE, LT, LE, GT, GE };

struct Cmp : Expr {
  std::unique_ptr<Expr> lhs, rhs;
  CmpOp op;
  Cmp(std::unique_ptr<Expr> L, CmpOp O, std::unique_ptr<Expr> R): lhs(std::move(L)), rhs(std::move(R)), op(O) {}
  bool eval(const RecordView& rv) const override;
};

enum class BoolOp { AND, OR };

struct BoolBin : Expr {
  std::unique_ptr<Expr> lhs, rhs;
  BoolOp op;
  BoolBin(std::unique_ptr<Expr> L, BoolOp O, std::unique_ptr<Expr> R): lhs(std::move(L)), rhs(std::move(R)), op(O) {}
  bool eval(const RecordView& rv) const override;
};

struct Not : Expr {
  std::unique_ptr<Expr> inner;
  explicit Not(std::unique_ptr<Expr> e): inner(std::move(e)) {}
  bool eval(const RecordView& rv) const override;
};

// ---------- Arithmetic ----------
enum class ArithOp { Add, Sub, Mul, Div };

struct Arith : Expr {
  std::unique_ptr<Expr> lhs, rhs;
  ArithOp op;
  Arith(std::unique_ptr<Expr> L, ArithOp O, std::unique_ptr<Expr> R)
    : lhs(std::move(L)), rhs(std::move(R)), op(O) {}
  bool   eval(const RecordView& rv) const override;   // truthy if numeric != 0
  double evalNumber(const RecordView& rv) const;      // numeric value
};

}} // namespace
