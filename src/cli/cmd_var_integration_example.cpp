// Example integration for an existing cmd_var.cpp.
// This is intentionally conservative because your exact cmd_VAR signature
// and expression-evaluation plumbing were not included in the conversation.
// Replace the body of your current variable-name parse/validate block with this pattern.

#include <iostream>
#include <sstream>
#include <string>

#include "dotscript_var_name.hpp"

namespace dottalk::dotscript {

// Helper: trim leading spaces before reading the remainder expression.
static std::string ltrim_copy(std::string s) {
    std::size_t i = 0;
    while (i < s.size() && static_cast<unsigned char>(s[i]) <= ' ') {
        ++i;
    }
    return s.substr(i);
}

// -----------------------------------------------------------------------------
// ASSUMED SHAPE ONLY:
// Adjust the function signature to match your existing cmd_var.cpp.
// -----------------------------------------------------------------------------
void cmd_VAR_example(std::istringstream& iss) {
    std::string var_name;
    if (!(iss >> var_name)) {
        std::cout << "Usage: VAR <name> = <expression>\n";
        return;
    }

    const VarNameCheck check = validate_var_name(var_name);
    if (!check.ok) {
        std::cout << check.message << "\n";
        return;
    }

    std::string eq;
    if (!(iss >> eq) || eq != "=") {
        std::cout << "Usage: VAR <name> = <expression>\n";
        return;
    }

    std::string expr;
    std::getline(iss, expr);
    expr = ltrim_copy(expr);
    if (expr.empty()) {
        std::cout << "Usage: VAR <name> = <expression>\n";
        return;
    }

    // -----------------------------------------------------------------
    // Replace this section with your real expression evaluation/storage:
    //   auto value = evaluate_expression(expr);
    //   vars_set(var_name, value);
    // -----------------------------------------------------------------
    std::cout << "VAR accepted: name='" << var_name << "' expr='" << expr << "'\n";
}

} // namespace dottalk::dotscript
