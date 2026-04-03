#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "dotscript_var_name.hpp"

using dottalk::dotscript::VarNameCheck;
using dottalk::dotscript::validate_var_name;

static std::string ltrim_copy(std::string s) {
    std::size_t i = 0;
    while (i < s.size() && static_cast<unsigned char>(s[i]) <= ' ') {
        ++i;
    }
    return s.substr(i);
}

void cmd_VAR(xbase::DbArea& current, std::istringstream& iss) {
    (void)current;

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

    // TODO: replace with your real variable storage/evaluation path.
    std::cout << "VAR accepted: name='" << var_name << "' expr='" << expr << "'\n";
}