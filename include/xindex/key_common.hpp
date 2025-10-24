#pragma once
#include <cstdint>
#include <vector>

namespace xindex {

using Key   = std::vector<std::uint8_t>;
using RecNo = std::uint32_t;

struct KeyLess {
    bool operator()(const Key& a, const Key& b) const noexcept {
        // std::vector is already lexicographic
        return a < b;
    }
};

} // namespace xindex
