// src/cli/cmd_ERROR_CLEAR.cpp
// Clear the last xBase_64 error and report success.

#include <iostream>
#include <sstream>

#include "xbase.hpp"
#include "xbase_error_codes.hpp"
#include "xbase_error_context.hpp"

using namespace xbase::error;

void cmd_ERROR_CLEAR(xbase::DbArea& A, std::istringstream& in)
{
    (void)A;
    (void)in;

    clear_last_error();

    std::cout << "Last error cleared.\n";
}