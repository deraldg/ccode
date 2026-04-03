#pragma once
#include <string>
#include <vector>
#include <optional>

namespace scanblock {

struct State {
    bool active = false;
    std::vector<std::string> lines;           // buffered body lines
    std::optional<std::string> for_expr;      // optional FOR <expr> on SCAN
};

inline State& state() {
    static State S;
    return S;
}

} // namespace scanblock



