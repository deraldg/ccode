#pragma once
// include/cli/order_hooks.hpp
// Lightweight mutation hooks that keep CLI-visible order state sane.

namespace xbase { class DbArea; }

namespace orderhooks {

// Called after any order/index mutation to reconcile per-area state.
// Must never throw.
void reconcile_after_mutation(xbase::DbArea& area) noexcept;

// Optional helper invoked by some commands after significant ops (best-effort).
void auto_top(xbase::DbArea& area) noexcept;

// Optional: attach a default order for an area (currently a no-op).
void attach_default_order(xbase::DbArea& area) noexcept;

} // namespace orderhooks
