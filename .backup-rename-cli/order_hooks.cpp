// src/cli/order_hooks.cpp
#include "order_hooks.hpp"

// Prefer the OOP path if available, otherwise fall back to legacy orderstate
#if __has_include("cli/table_context.hpp")
  #include "cli/table_context.hpp"
  #define DOTTALK_HAS_TABLE_CONTEXT 1
#else
  #include "order_state.hpp"
  #define DOTTALK_HAS_TABLE_CONTEXT 0
#endif

#include "xbase.hpp"
#include "xindex/index_manager.hpp"
#include "xindex/attach.hpp"

#include <filesystem>
#include <string>

namespace order_hooks {

// --- helper: rebuild all in-memory indexes (identity recno remap) ---
static void rebuild_all_indexes(xbase::DbArea& a) noexcept {
    try {
        auto& mgr = xindex::ensure_manager(a);
        // Identity mapping is fine for generic refresh; it fully rebuilds.
        mgr.on_pack([](int i){ return i; });
    } catch (...) {
        // no manager attached yet; ignore
    }
}

// Attach a default order if there is exactly one .inx next to the DBF
void attach_default_order(xbase::DbArea& a) {
    if (!a.isOpen()) return;
    const std::string dbname = a.name();
    if (dbname.empty()) return;

    namespace fs = std::filesystem;

    // Already has an order selected? If so, do nothing.
#if DOTTALK_HAS_TABLE_CONTEXT
    auto& ord = cli::ensure_table(a).order();
    if (!ord.orderName().empty()) return;
#else
    if (!orderstate::orderName(a).empty()) return;
#endif

    // Look next to the DBF; if there's exactly one .inx, attach it
    fs::path dir = fs::path(dbname).parent_path();
    if (dir.empty()) return; // avoid scanning CWD when path is relative/unknown

    fs::path only;
    size_t count = 0;
    try {
        for (auto const& de : fs::directory_iterator(dir)) {
            if (de.is_regular_file() && de.path().extension() == ".inx") {
                only = de.path();
                if (++count > 1) break;
            }
        }
    } catch (...) {
        return; // directory not accessible; just bail
    }

    if (count == 1) {
#if DOTTALK_HAS_TABLE_CONTEXT
        ord.setOrder(only.string());
#else
        orderstate::setOrder(a, only.string());
#endif
    }
}

// Notify that data changed in-place (append/replace/delete/recall unknown):
// safest action is to rebuild in-memory indexes.
void order_notify_mutation(xbase::DbArea& a) noexcept {
    rebuild_all_indexes(a);
}

// Notify that a manual REFRESH happened — rebuild to sync with table
void order_notify_refresh(xbase::DbArea& a) {
    rebuild_all_indexes(a);
}

// Notify that PACK happened — indexes are stale; clear and reattach default if any
void order_notify_pack(xbase::DbArea& a) {
    try {
        auto& mgr = xindex::ensure_manager(a);
        mgr.on_zap();
    } catch (...) { /* ignore */ }
    attach_default_order(a);
}

// Optional: after index changes, some flows auto-top/bottom — keep as no-op for now
void order_auto_top(xbase::DbArea& /*a*/) {
    // Intentionally empty; explicit TOP/BOTTOM commands handle navigation.
}

} // namespace order_hooks

// ---- Global wrappers for legacy call sites (keep binary linkage stable) ----
void attach_default_order(xbase::DbArea& a) {
    order_hooks::attach_default_order(a);
}
void order_notify_mutation(xbase::DbArea& a) noexcept {
    order_hooks::order_notify_mutation(a);
}
void order_notify_refresh(xbase::DbArea& a) {
    order_hooks::order_notify_refresh(a);
}
void order_notify_pack(xbase::DbArea& a) {
    order_hooks::order_notify_pack(a);
}
void order_auto_top(xbase::DbArea& a) noexcept {
    order_hooks::order_auto_top(a);
}
