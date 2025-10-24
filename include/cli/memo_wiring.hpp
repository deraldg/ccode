#pragma once
// include/cli/memo_wiring.hpp
//
// CLI glue for memo fields + compatibility shims so existing cmd_replace.cpp builds.
// TODOs are safe stubs now; wire them to your schema/row codec when ready.

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

#include "xbase.hpp"
#include "memo/memostore.hpp"
#include "cli/memo_auto.hpp"  // memo_store_for(...)

namespace cli_memo {

// ---- Wiring points you will implement later --------------------------------

// Return true if <fieldName> is a memo field in the current table schema.
inline bool field_is_memo(xbase::DbArea& area, const std::string& fieldName) {
    (void)area; (void)fieldName;
    // TODO: area.table().find_field_ci(fieldName)->type == FieldType::Memo
    return false;
}

// Set/replace memo bytes for <fieldName> on the *current record*.
// Must set the record’s memo pointer (u64 id) and mark the row dirty.
inline bool set_current_record_memo(
    xbase::DbArea& area,
    const std::string& fieldName,
    uint64_t object_id,
    uint32_t length,
    std::string* err)
{
    (void)area; (void)fieldName; (void)object_id; (void)length;
    if (err) *err = "set_current_record_memo: not wired to record codec.";
    return false;
}

// Clear memo for <fieldName> on the current record (zero pointer, optional erase).
inline bool clear_current_record_memo(
    xbase::DbArea& area,
    const std::string& fieldName,
    std::string* err)
{
    (void)area; (void)fieldName;
    if (err) *err = "clear_current_record_memo: not wired to record codec.";
    return false;
}

// Fetch current record’s memo id. Return true if wired (even if id==0).
inline bool try_get_current_record_memo_id(
    xbase::DbArea& area,
    const std::string& fieldName,
    uint64_t& out_id)
{
    (void)area; (void)fieldName;
    out_id = 0;
    // TODO: out_id = row.get_memo_id(fieldName);
    return false;
}

// Non-owning access to the already-open store for this area.
inline xbase::memo::MemoStore* require_memo_store(xbase::DbArea& area, std::string* err) {
    auto* st = cli_memo::memo_store_for(area);
    if (!st && err) *err = "Memo sidecar is not open for this work area.";
    return st;
}

} // namespace cli_memo

// ---- Compatibility shims for existing cmd_replace.cpp -----------------------

namespace cli_memo {

struct _NoDel { void operator()(xbase::memo::MemoStore*) const noexcept {} };
using MemoStoreHandle = std::unique_ptr<xbase::memo::MemoStore, _NoDel>;

// Legacy acquire_memo_store(...) used by your current cmd_replace.cpp
inline MemoStoreHandle acquire_memo_store(
    xbase::DbArea& area,
    bool /*autoCreate*/,
    std::string* err)
{
    auto* st = cli_memo::memo_store_for(area);
    if (!st) {
        if (err) *err = "Memo sidecar is not open for this work area.";
        return MemoStoreHandle{};
    }
    return MemoStoreHandle(st); // non-owning
}

// Legacy clear_current_record_memo(...) overload that takes a handle.
inline bool clear_current_record_memo(
    xbase::DbArea& area,
    const std::string& fieldName,
    MemoStoreHandle& /*store*/,
    std::string* err)
{
    return clear_current_record_memo(area, fieldName, err);
}

} // namespace cli_memo
