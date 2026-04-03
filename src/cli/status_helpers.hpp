#pragma once
#include <string>
#include "xbase.hpp"

namespace status {
    // Compact single-line order summary used by STATUS header
    std::string format_active_order(const xbase::DbArea& A);
}



