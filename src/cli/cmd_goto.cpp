// src/cli/cmd_goto.cpp
// Narrow, engine-facing GOTO command.
//
// Supported forms:
//   GOTO <recno>
//   GOTO FIRST
//   GOTO LAST

#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cli/cmd_nav_move.hpp"

void cmd_GOTO(xbase::DbArea& A, std::istringstream& iss)
{
    std::string tok;
    if (!(iss >> tok)) {
        std::cout << "Usage: GOTO <recno> or GOTO FIRST|LAST\n";
        return;
    }

    const std::string u = cli::nav::upper_copy(tok);
    if (u == "FIRST") {
        cli::nav::go_endpoint(A, cli::nav::Endpoint::First, "GOTO");
        return;
    }
    if (u == "LAST") {
        cli::nav::go_endpoint(A, cli::nav::Endpoint::Last, "GOTO");
        return;
    }

    int n = 0;
    if (!cli::nav::try_parse_int_token(tok, n) || n <= 0) {
        std::cout << "Usage: GOTO <recno> or GOTO FIRST|LAST\n";
        return;
    }

    cli::nav::go_absolute(A, n, "GOTO");
}
