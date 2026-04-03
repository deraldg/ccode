// src/cli/cmd_if.cpp
// IF / ELSE / ENDIF
//
// Thin command layer. Control logic is owned by shell_control_utils,
// which must be the same stack consulted by shell_execute_line(...).

#include "xbase.hpp"
#include "cli_comment.hpp"
#include "shell.hpp"
#include "shell_control_utils.hpp"

#include <iostream>
#include <sstream>
#include <string>

static std::string trim_local(std::string s)
{
    auto issp = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
}

void cmd_IF(xbase::DbArea& area, std::istringstream& in)
{
    std::string expr;
    std::getline(in, expr);
    expr = cliutil::strip_inline_comments(trim_local(expr));

    // Already suppressed by outer IF -> do not evaluate.
    if (!dottalk::shell_if_can_eval()) {
        dottalk::shell_if_enter(false, /*evaluated=*/false);
        return;
    }

    if (expr.empty()) {
        std::cout << "IF error: missing expression\n";
        dottalk::shell_if_enter(false, /*evaluated=*/false);
        return;
    }

    bool cond = false;
    std::string err;
    const bool ok = shell_eval_bool_expr(area, expr, &cond, &err);
    if (!ok) {
        std::cout << "IF error: "
                  << (err.empty() ? "invalid boolean expression" : err) << "\n";
        dottalk::shell_if_enter(false, /*evaluated=*/false);
        return;
    }

    dottalk::shell_if_enter(cond, /*evaluated=*/true);
}

void cmd_ELSE(xbase::DbArea&, std::istringstream&)
{
    dottalk::shell_if_else();
}

void cmd_ENDIF(xbase::DbArea&, std::istringstream&)
{
    dottalk::shell_if_exit();
}
