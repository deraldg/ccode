// src/cli/cmd_append_blank.cpp — APPEND BLANK
//
// Delegates to shared append support so APPEND and APPEND BLANK stay aligned.

#include <sstream>

#include "xbase.hpp"
#include "cli/append_support.hpp"

void cmd_APPEND_BLANK(xbase::DbArea& A, std::istringstream& iss)
{
    (void)dottalk_append_blank_core(A, iss);
}