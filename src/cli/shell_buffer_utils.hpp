#pragma once

#include "xbase.hpp"  // xbase::XBaseEngine, xbase::DbArea
#include <string>

namespace dottalk {

inline bool is_match(const std::string& u, const char* a, const char* b) {
    return u == a || u == b;
}

bool handle_buffers_if_active(xbase::XBaseEngine& eng,
                              const std::string& U,
                              const std::string& line_for_scan,
                              const std::string& line_for_loop);

} // namespace dottalk