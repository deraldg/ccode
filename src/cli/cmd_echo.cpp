// src/cli/cmd_echo.cpp
#include "xbase.hpp"
#include <sstream>
#include <iostream>
#include <string>

#include "cli/output_router.hpp"

// ECHO behavior requirement:
// - Default: SET ECHO OFF (router default)
// - When ECHO is OFF: ECHO <string> is ignored by the system and becomes
//   a comment string to console.
// - When ECHO is ON: output is routed normally (console/print/alternate apply).
//
// Important:
// - "ECHO ON" is NOT the toggle command.
// - Use "SET ECHO ON|OFF" to change router echo mode.

void cmd_ECHO(xbase::DbArea&, std::istringstream& iss)
{
    std::string rest;
    std::getline(iss, rest);
    if (!rest.empty() && rest.front() == ' ')
        rest.erase(0, 1);

    auto& R = cli::OutputRouter::instance();

    if (!R.echo_on()) {
        // Comment-style console output, forced directly to console.
        // This intentionally bypasses router switches so comments remain visible
        // even if SET CONSOLE OFF or alternate/print routing changes.
        if (!rest.empty()) std::cout << "; " << rest << "\n";
        else               std::cout << ";\n";
        return;
    }

    // Echo ON: route normally through the current output paths.
    R.out() << rest << "\n";
}