#pragma once
#include <string>
#include "xbase.hpp"

std::string normalize_unquoted_rhs_literals(xbase::DbArea& area,
                                            const std::string& expr);