#pragma once

#include <vector>
#include <string>

namespace dottalk {

// IF / ELSE / ENDIF stack frame
struct IfFrame {
    bool condition;
    bool evaluated;
    bool in_else;
};

// Global IF stack
extern std::vector<IfFrame> g_if_stack;

// Check if current line is an IF control command
bool is_if_control_command(const std::string& U);

// Compute if execution is suppressed (inside false IF/ELSE)
bool compute_if_suppressed();

// IF control flow API
bool shell_if_can_eval();
bool shell_if_is_suppressed();
void shell_if_enter(bool condition, bool evaluated);
void shell_if_else();
void shell_if_exit();

} // namespace dottalk