// src/cli/cmd_go.cpp
// FoxPro-style GO router.
//
// Supported forms:
//   GO
//   GO TOP | BOTTOM | FIRST | LAST
//   GO [TO] <recno>
//   GO RECORD <recno>
//   GO +/-<n>

#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "cli/cmd_nav_move.hpp"

void cmd_GO(xbase::DbArea& A, std::istringstream& in)
{
    std::string tok;
    if (!(in >> tok)) {
        // Placeholder for relationship refresh; for now, just re-read current.
        cli::nav::refresh_current(A, "GO");
        return;
    }

    const std::string u = cli::nav::upper_copy(tok);

    if (u == "TOP") {
        cli::nav::go_endpoint(A, cli::nav::Endpoint::Top, "GO");
        return;
    }
    if (u == "BOTTOM") {
        cli::nav::go_endpoint(A, cli::nav::Endpoint::Bottom, "GO");
        return;
    }
    if (u == "FIRST") {
        cli::nav::go_endpoint(A, cli::nav::Endpoint::First, "GO");
        return;
    }
    if (u == "LAST") {
        cli::nav::go_endpoint(A, cli::nav::Endpoint::Last, "GO");
        return;
    }

    if (u == "TO" || u == "RECORD") {
        std::string nTok;
        int n = 0;
        if (!(in >> nTok) || !cli::nav::try_parse_int_token(nTok, n) || n <= 0) {
            std::cout << "GO: expected a positive record number after " << u << ".\n";
            return;
        }
        cli::nav::go_absolute(A, n, "GO");
        return;
    }

    if (u == "IN") {
        std::cout << "GO: 'IN <alias>' not supported yet (SELECT the area, then GO ...).\n";
        return;
    }

    int n = 0;
    if (cli::nav::try_parse_int_token(tok, n)) {
        if (!tok.empty() && (tok[0] == '+' || tok[0] == '-')) {
            cli::nav::skip_relative(A, n, "GO");
        } else {
            cli::nav::go_absolute(A, n, "GO");
        }
        return;
    }

    std::cout << "GO: unrecognized form. Use GO, GO TOP/BOTTOM/FIRST/LAST, GO [TO] n, GO RECORD n, or GO +/-n.\n";
}
