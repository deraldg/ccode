#include "dli/browse_edit.hpp"
#include "dli/replace_api.hpp"

#include <sstream>
#include <stdexcept>

namespace dli {

static Hooks g_hooks{}; // default-empty hooks

void set_hooks(const Hooks& h) { g_hooks = h; }

EditSession begin_edit(xbase::DbArea& db) {
    EditSession es;
    es.db = &db;
    // We assume DbArea has recno() accessor; if not, adapt here.
    // If you need a different call, wrap it with a small inline helper.
    // e.g., es.recno = dli_current_recno(db);
    extern long long dli_current_recno(xbase::DbArea&); // provide in your host app if you don't have DbArea::recno()
    try {
        // Prefer a direct member if available; otherwise fall back to external helper.
        // NOTE: Remove the next two lines if your DbArea has recno().
        // (The extern is only used if you provide it.)
        es.recno = dli_current_recno(db);
    } catch (...) {
        // If an extern helper isn't provided, we just leave recno as 0.
    }
    es.active = true;
    return es;
}

bool stage(EditSession& es, const std::string& field, const std::string& raw) {
    if (!es.active || !es.db) return false;
    es.staged[field] = raw;
    return true;
}

static bool is_memo(xbase::DbArea& db, const std::string& field) {
    if (g_hooks.field_type) {
        char t = g_hooks.field_type(db, field);
        return t == 'M' || t == 'm';
    }
    return false;
}

bool commit(EditSession& es, std::string& err) {
    if (!es.active || !es.db) { err = "No active edit session."; return false; }
    auto& db = *es.db;

    // Apply staged changes using the real REPLACE handler so all hooks/index logic fires.
    for (const auto& kv : es.staged) {
        const std::string& field = kv.first;
        const std::string& raw   = kv.second;

        bool ok = false;
        if (is_memo(db, field)) {
            ok = dli::do_replace_memo_text(db, field, raw, &err);
        } else {
            ok = dli::do_replace_text(db, field, raw, &err);
        }
        if (!ok) {
            if (err.empty()) err = std::string("Failed to update field '") + field + "'.";
            return false;
        }
    }

    es.active = false;
    es.staged.clear();

    // Give host a chance to reseek if order/filter changed
    bool repositioned = false;
    if (g_hooks.after_commit_reposition) {
        repositioned = g_hooks.after_commit_reposition(db, es.recno, es.staged);
    }

    // Always request a UI refresh (host can no-op this)
    if (g_hooks.refresh_row) {
        g_hooks.refresh_row(db);
    }

    (void)repositioned; // placeholder; in case you want to branch behavior

    return true;
}

void cancel(EditSession& es) {
    es.active = false;
    es.staged.clear();
}

} // namespace dli



