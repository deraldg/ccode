#pragma once
#include <string>
#include <cstdint>

namespace xbase { class DbArea; }

namespace xbase::locks {

// -------- Owner identity -----------------------------------------------------
// A comparable token for "who owns the lock". Use a stable per-process/session id.
struct Owner {
    std::string id;            // e.g., "host:pid:nonce"
    bool operator==(const Owner& o) const noexcept { return id == o.id; }
    bool operator!=(const Owner& o) const noexcept { return id != o.id; }
};

// Returns the process/session owner token (singleton per process).
const Owner& current_owner();

// -------- Table locks --------------------------------------------------------
// New owner-aware API:
bool try_lock_table(DbArea& a, const Owner& owner, std::string* err = nullptr);
bool unlock_table   (DbArea& a, const Owner& owner, std::string* err = nullptr);
bool is_table_locked(const DbArea& a, std::string* owner_out); // owner string if locked

// Back-compat shims (behave as "current_owner"):
bool try_lock_table(DbArea& a, std::string* err = nullptr);
void unlock_table   (DbArea& a);                 // best-effort: ignores failures
bool is_table_locked(const DbArea& a);           // no owner info

// -------- Record locks -------------------------------------------------------
// New owner-aware API:
bool try_lock_record(DbArea& a, uint32_t recno, const Owner& owner, std::string* err = nullptr);
bool unlock_record  (DbArea& a, uint32_t recno, const Owner& owner, std::string* err = nullptr);
bool is_record_locked(const DbArea& a, uint32_t recno, std::string* owner_out); // owner string if locked

// Back-compat shims:
bool try_lock_record(DbArea& a, uint32_t recno, std::string* err = nullptr);
void unlock_record  (DbArea& a, uint32_t recno); // best-effort
bool is_record_locked(const DbArea& a, uint32_t recno);

// -------- Admin / recovery (optional) ---------------------------------------
// Force unlock ignores ownership (use sparingly / logged by caller).
bool force_unlock_table (DbArea& a, std::string* err = nullptr);
bool force_unlock_record(DbArea& a, uint32_t recno, std::string* err = nullptr);

// Cleanup any locks created by this process for this area (best-effort)
void release_held(DbArea& a);

} // namespace xbase::locks
