#pragma once

#include <string_view>

// -----------------------------------------------------------------------------
// Relations auto-refresh suppression (shared utility)
//
// Purpose:
//   Provide a thread-local suppression counter plus a small RAII guard.
//
// ABI notes:
//   The push/pop/is_suspended functions are `extern "C"` to preserve the
//   existing linkage expectations from legacy shell code and any external users.
// -----------------------------------------------------------------------------

extern "C" void shell_rel_refresh_push() noexcept;
extern "C" void shell_rel_refresh_pop() noexcept;
extern "C" bool shell_rel_refresh_is_suspended() noexcept;

// Return true if command token U should suppress auto REL refresh.
bool shell_is_rel_refresh_suppression_command(std::string_view U) noexcept;

// Simple RAII guard.
struct RelRefreshGuard {
    bool active;

    explicit RelRefreshGuard(bool on) noexcept
        : active(on)
    {
        if (active) shell_rel_refresh_push();
    }

    RelRefreshGuard(const RelRefreshGuard&) = delete;
    RelRefreshGuard& operator=(const RelRefreshGuard&) = delete;

    RelRefreshGuard(RelRefreshGuard&& other) noexcept
        : active(other.active)
    {
        other.active = false;
    }

    RelRefreshGuard& operator=(RelRefreshGuard&& other) noexcept
    {
        if (this != &other) {
            if (active) shell_rel_refresh_pop();
            active = other.active;
            other.active = false;
        }
        return *this;
    }

    ~RelRefreshGuard() noexcept
    {
        if (active) shell_rel_refresh_pop();
    }
};
