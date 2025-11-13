#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <optional>
#include <cmath>

#include "dottalk/expr/api.hpp"
#include "dottalk/expr/ast.hpp"
#include "dottalk/expr/eval.hpp"
#include "dottalk/expr/for_parser.hpp"

using dottalk::expr::CompileResult;
using dottalk::expr::Expr;
using dottalk::expr::Arith;
using dottalk::expr::LitNumber;
using dottalk::expr::LitString;
using dottalk::expr::RecordView;

namespace {
struct TestRunner {
  int passed=0, failed=0;
  void expect(bool c, const char* e, const char* f, int l){ if(c)++passed; else{++failed; std::cerr<<f<<":"<<l<<": EXPECT "<<e<<"\n";}}
  template<class T,class U> void expect_eq(const T&a,const U&b,const char*e,const char*f,int l){
    if(a==b)++passed; else{++failed; std::cerr<<f<<":"<<l<<": EXPECT_EQ "<<e<<" got:["<<a<<"] want:["<<b<<"]\n";}
  }
  void summary()const{ std::cout<<"[expr_tests] passed="<<passed<<" failed="<<failed<<"\n"; }
};
#define EXPECT(x) runner.expect((x), #x, __FILE__, __LINE__)
#define EXPECT_EQ(a,b) runner.expect_eq((a),(b), #a " == " #b, __FILE__, __LINE__)

struct MapRecord { std::unordered_map<std::string,std::string> fields; };

RecordView make_rv(const MapRecord& rec){
  RecordView rv;
  rv.get_field_str = [&](std::string_view n)->std::string{
    auto it=rec.fields.find(std::string(n)); return it==rec.fields.end()? std::string(): it->second;
  };
  rv.get_field_num = [&](std::string_view n)->std::optional<double>{
    auto it=rec.fields.find(std::string(n)); if(it==rec.fields.end()) return std::nullopt;
    return dottalk::expr::to_number(it->second);
  };
  return rv;
}

CompileResult C(const std::string& s){ return dottalk::expr::compile_where(s); }

double eval_number(const std::unique_ptr<Expr>& e, const RecordView& rv){
  if(auto ar = dynamic_cast<Arith*>(e.get())) return ar->evalNumber(rv);
  if(auto ln = dynamic_cast<LitNumber*>(e.get())) return ln->v;
  if(auto ls = dynamic_cast<LitString*>(e.get())) return dottalk::expr::to_number(ls->v).value_or(NAN);
  return e->eval(rv) ? 1.0 : 0.0;
}
} // namespace

int main(){
  TestRunner runner;

  { auto cr=C("1 + 2 * 3"); EXPECT(cr.program); MapRecord r{}; auto rv=make_rv(r); EXPECT_EQ(eval_number(cr.program, rv), 7.0); }
  { auto cr=C("(1 + 2) * 3"); EXPECT(cr.program); MapRecord r{}; auto rv=make_rv(r); EXPECT_EQ(eval_number(cr.program, rv), 9.0); }
  { auto cr=C("-5 + 2"); EXPECT(cr.program); MapRecord r{}; auto rv=make_rv(r); EXPECT_EQ(eval_number(cr.program, rv), -3.0); }
  { auto cr=C("age >= 21"); EXPECT(cr.program); MapRecord r{{{"age","22"}}}; auto rv=make_rv(r); EXPECT(cr.program->eval(rv)); }
  { auto cr=C("lname = \"WHITE\""); EXPECT(cr.program); MapRecord r{{{"lname","White"}}}; auto rv=make_rv(r); EXPECT(cr.program->eval(rv)); }
  { auto cr=C("A = \"x\" OR B = \"y\" AND C = \"z\""); EXPECT(cr.program); MapRecord r{{{"A","x"},{"B","n"},{"C","n"}}}; auto rv=make_rv(r); EXPECT(cr.program->eval(rv)); }
  { auto cr=C("lname = \"White\" AND gpa > 3 OR (gpa + age > 40)"); EXPECT(cr.program); MapRecord r{{{"lname","White"},{"gpa","3.1"},{"age","38"}}}; auto rv=make_rv(r); EXPECT(cr.program->eval(rv)); }
  { auto cr=C("10 / 0"); EXPECT(cr.program); MapRecord r{}; auto rv=make_rv(r); EXPECT_EQ(eval_number(cr.program, rv), 0.0); }
  { auto cr=C("gpa + 1"); EXPECT(cr.program); MapRecord r{{{"gpa","3.50"}}}; auto rv=make_rv(r); EXPECT_EQ(eval_number(cr.program, rv), 4.5); }
  { auto cr=C("code < \"ZZZ\""); EXPECT(cr.program); MapRecord r{{{"code","ABC"}}}; auto rv=make_rv(r); EXPECT(cr.program->eval(rv)); }
  { std::istringstream iss("FOR lname = \"White\" AND gpa > 3"); std::string out; bool ok=dottalk::expr::extract_for_clause(iss,out); EXPECT(ok); EXPECT_EQ(out, "lname = \"White\" AND gpa > 3"); }
  { auto cr=C("NOT (gpa > 3)"); EXPECT(cr.program); MapRecord r{{{"gpa","2.5"}}}; auto rv=make_rv(r); EXPECT(cr.program->eval(rv)); }
  { auto cr=C("(gpa * 10) >= 35"); EXPECT(cr.program); MapRecord r{{{"gpa","3.6"}}}; auto rv=make_rv(r); EXPECT(cr.program->eval(rv)); }

  runner.summary();
  return 0;
}
