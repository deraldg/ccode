#include "shell_control_utils.hpp"

#include <string>

namespace dottalk {

// Global IF stack
std::vector<IfFrame> g_if_stack;

bool is_if_control_command(const std::string& U)
{
    return (U == "IF" || U == "ELSE" || U == "ENDIF");
}

bool compute_if_suppressed()
{
    for (const auto& f : g_if_stack) {
        if (!f.evaluated) return true;
        if (!f.in_else) {
            if (!f.condition) return true;
        } else {
            if (f.condition) return true;
        }
    }
    return false;
}

bool shell_if_can_eval() { return !compute_if_suppressed(); }
bool shell_if_is_suppressed() { return compute_if_suppressed(); }

void shell_if_enter(bool condition, bool evaluated) {
    g_if_stack.push_back({condition, evaluated, false});
}

void shell_if_else() {
    if (!g_if_stack.empty()) g_if_stack.back().in_else = true;
}

void shell_if_exit() {
    if (!g_if_stack.empty()) g_if_stack.pop_back();
}

} // namespace dottalk