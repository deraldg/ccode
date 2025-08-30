// src/cli/cmd_setcase.cpp
#include <iostream>
#include <sstream>
#include <string>

#include "command_registry.hpp"
#include "predicate_eval.hpp"
#include "textio.hpp"
#include "xbase.hpp"

void cmd_SETCASE(xbase::DbArea&, std::istringstream& iss) {
    std::string tok;
    if (!(iss >> tok)) {
        std::cout << (predx::get_case_sensitive() ? "CASE SENSITIVE: ON\n" : "CASE SENSITIVE: OFF\n");
        return;
    }
    if (textio::ieq(tok, "ON") || textio::ieq(tok, "TRUE") || textio::ieq(tok, "1")) {
        predx::set_case_sensitive(true);
        std::cout << "CASE SENSITIVE: ON\n";
    } else if (textio::ieq(tok, "OFF") || textio::ieq(tok, "FALSE") || textio::ieq(tok, "0")) {
        predx::set_case_sensitive(false);
        std::cout << "CASE SENSITIVE: OFF\n";
    } else {
        std::cout << "Usage: SETCASE [ON|OFF]\n";
    }
}

static bool s_reg = [](){
    cli::registry().add("SETCASE", &cmd_SETCASE);
    return true;
}();
