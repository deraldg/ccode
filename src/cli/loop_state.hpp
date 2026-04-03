// ============================================================================
// path: src/cli/loop_state.hpp
// purpose: Shared loop-capture state for LOOP / WHILE / UNTIL (header-only)
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <optional>

namespace loopblock {

struct State {
    bool active{false};                        // capturing?
    bool quiet{false};                         // suppress chatter
    std::optional<std::string> for_expr{};     // optional FOR/cond label
    std::vector<std::string> lines;            // buffered body
};

// Header-only singleton accessor (fixes unresolved external).
inline State& state() {
    static State g_state;
    return g_state;
}

// Convenience: clear buffer + flags.
inline void clear() {
    State& s = state();
    s.active = false;
    s.quiet = false;
    s.for_expr.reset();
    s.lines.clear();
}

} // namespace loopblock



