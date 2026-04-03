// src/cli/cmd_evaluate.cpp
// DotTalk++ EVALUATE / EVAL
//
// Boolean predicate evaluator (IF/SCAN compatible)

#include "xbase.hpp"
#include "cli_comment.hpp"
#include "cli/expr/evaluate.hpp"

#include <iostream>
#include <sstream>
#include <string>

using namespace xbase;

namespace {

static std::string trim_local(std::string s)
{
    auto issp = [](unsigned char c){
        return c==' ' || c=='\t' || c=='\r' || c=='\n';
    };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}

} // namespace

void cmd_EVALUATE(DbArea& area, std::istringstream& in)
{
    std::string expr;
    std::getline(in, expr);
    expr = cliutil::strip_inline_comments(trim_local(expr));

    if (expr.empty()) {
        std::cout << "Usage: EVALUATE <expr>\n";
        return;
    }

    auto co = dottalk::expr::compile_predicate(expr, area);
    if (!co.pred) {
        std::cout << "EVALUATE error: " << co.error << "\n";
        return;
    }

    try {
        bool result = co.pred.eval(co.pred.impl, area);
        std::cout << (result ? ".T." : ".F.") << "\n";
    } catch (const std::exception& e) {
        std::cout << "EVALUATE error: " << e.what() << "\n";
    } catch (...) {
        std::cout << "EVALUATE error: evaluation failed.\n";
    }
}
