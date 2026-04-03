// ==============================
// File: src/xbase/cursor_hook.cpp
// ==============================
#include "cursor_hook.hpp"
#include "xbase.hpp"

#include <atomic>

namespace xbase::cursor_hook {

static std::atomic<Callback> g_cb{nullptr};
static std::atomic<void*>    g_user{nullptr};
static thread_local int      g_suppress_depth = 0;

void set_callback(Callback cb, void* user) noexcept {
    g_user.store(user, std::memory_order_relaxed);
    g_cb.store(cb, std::memory_order_release);
}

void notify(DbArea& area, const char* reason) noexcept {
    if (g_suppress_depth > 0) return;
    Callback cb = g_cb.load(std::memory_order_acquire);
    if (!cb) return;
    void* user = g_user.load(std::memory_order_relaxed);
    cb(area, reason, user);
}

Guard::Guard() noexcept  { ++g_suppress_depth; }
Guard::~Guard() noexcept { --g_suppress_depth; }

} // namespace xbase::cursor_hook
