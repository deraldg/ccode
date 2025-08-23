#pragma once
// src/cli/order_hooks.hpp
// Safe hooks for index/order notifications.
// v3: provide real implementations in order_hooks.cpp that call existing commands.

namespace xbase { class DbArea; }

// Called after data mutations (APPEND/DELETE/RECALL/PACK/etc.)
void order_notify_mutation(xbase::DbArea&) noexcept;

// Called right after building an index to move cursor to first record (TOP).
void order_auto_top(xbase::DbArea&) noexcept;
