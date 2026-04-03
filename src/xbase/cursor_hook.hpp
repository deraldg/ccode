// ==============================
// File: src/xbase/cursor_hook.hpp
// ==============================
#pragma once
#include <cstddef>

namespace xbase {
class DbArea;

namespace cursor_hook {

using Callback = void(*)(DbArea& area, const char* reason, void* user) noexcept;

// Set once during startup (shell).
void set_callback(Callback cb, void* user) noexcept;

// Called by DbArea movement/edit methods.
void notify(DbArea& area, const char* reason) noexcept;

// Optional: suppress notify within a scope.
class Guard {
public:
    Guard() noexcept;
    ~Guard() noexcept;

    Guard(const Guard&)            = delete;
    Guard& operator=(const Guard&) = delete;
    Guard(Guard&&)                 = delete;
    Guard& operator=(Guard&&)      = delete;
};

} // namespace cursor_hook
} // namespace xbase



