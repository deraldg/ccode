#pragma once
#include <string>
#include <unordered_map>
#include <functional>

namespace xbase { class DbArea; } // forward declaration

namespace dli {

// Lightweight edit-session container used by browsetui
struct EditSession {
    xbase::DbArea* db {nullptr};
    long long recno {0};
    std::unordered_map<std::string, std::string> staged; // field -> raw text as shown in UI
    bool active {false};
};

// Optional integration hooks supplied by the host app (you)
struct Hooks {
    // Return field type as FoxPro-style code: 'C','N','F','D','L','M' (or 0 if unknown)
    std::function<char(xbase::DbArea&, const std::string& fieldName)> field_type;

    // After successful commit, you can reposition (e.g., re-SEEK on key change). 
    // Return true if you handled repositioning; false to use default behavior.
    std::function<bool(xbase::DbArea&, long long /*original_recno*/,
                       const std::unordered_map<std::string,std::string>& /*staged*/)> after_commit_reposition;

    // Redraw/refresh the current row in browsetui.
    std::function<void(xbase::DbArea&)> refresh_row;
};

// Install/replace hooks (call once at app init, or anytime to update)
void set_hooks(const Hooks& h);

// Start an edit session on the current record
EditSession begin_edit(xbase::DbArea& db);

// Stage a field change from the UI (raw text). Returns false if no active session.
bool stage(EditSession& es, const std::string& fieldName, const std::string& rawText);

// Validate + commit all staged changes through the real DLI/command API (REPLACE).
// On failure, returns false and sets 'err' with a human-readable message.
bool commit(EditSession& es, std::string& err);

// Discard staged changes
void cancel(EditSession& es);

} // namespace dli
