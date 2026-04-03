// src/cli/cmd_boolean.cpp
#include "xbase.hpp"
#include "cli_comment.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

// DotTalk++ expression engine
#include "cli/expr/api.hpp"
#include "cli/expr/ast.hpp"
#include "cli/expr/glue_xbase.hpp"

using namespace xbase;

namespace {

static std::string trim_local(std::string s) {
    auto issp = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
}

} // namespace

void cmd_BOOLEAN(DbArea& area, std::istringstream& in) {
    std::string expr;
    std::getline(in, expr);
    expr = cliutil::strip_inline_comments(trim_local(expr));

    if (expr.empty()) {
        std::cout << "Usage: BOOLEAN <expr>\n";
        return;
    }

    auto cr = dottalk::expr::compile_where(expr);
    if (!cr.program) {
        std::cout << "BOOLEAN error: " << cr.error << "\n";
        return;
    }

    auto rv = dottalk::expr::glue::make_record_view(area);

    try {
        bool result = cr.program->eval(rv);
        std::cout << (result ? ".T." : ".F.") << "\n";
    } catch (const std::exception& e) {
        std::cout << "BOOLEAN error: " << e.what() << "\n";
    } catch (...) {
        std::cout << "BOOLEAN error: evaluation failed.\n";
    }
}



