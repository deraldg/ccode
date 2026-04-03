#pragma once
// include/cli/memo_wiring.hpp
//
// CLI glue for memo fields + compatibility shims so existing cmd_replace.cpp builds.
// TODO stubs are intentionally safe for now.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "memo_backend.hpp"
#include "memo_auto.hpp"  // memo_backend_for(...)

namespace cli_memo {

// ---- Wiring points you will implement later --------------------------------

// Return true if <fieldName> is a memo field in the current table schema.
inline bool field_is_memo(xbase::DbArea& area, const std::string& fieldName) {
    const auto defs = area.fields();
    for (const auto& f : defs) {
        if (f.name == fieldName) {
            return f.type == 'M' || f.type == 'm';
        }
    }
    return false;
}

// Set/replace memo bytes for <fieldName> on the current record.
// Must eventually set the record's memo token/pointer in the DBF row.
inline bool set_current_record_memo(
    xbase::DbArea& area,
    const std::string& fieldName,
    std::uint64_t object_id,
    std::uint32_t length,
    std::string* err)
{
    (void)area;
    (void)fieldName;
    (void)object_id;
    (void)length;
    if (err) *err = "set_current_record_memo: not wired to record codec.";
    return false;
}

// Clear memo for <fieldName> on the current record.
inline bool clear_current_record_memo(
    xbase::DbArea& area,
    const std::string& fieldName,
    std::string* err)
{
    (void)area;
    (void)fieldName;
    if (err) *err = "clear_current_record_memo: not wired to record codec.";
    return false;
}

// Fetch current record's memo id. Return true if wired (even if id==0).
inline bool try_get_current_record_memo_id(
    xbase::DbArea& area,
    const std::string& fieldName,
    std::uint64_t& out_id)
{
    (void)area;
    (void)fieldName;
    out_id = 0;
    // TODO: decode current row's memo token/pointer when row codec is wired.
    return false;
}

// Non-owning access to the already-open backend for this area.
inline dottalk::memo::IMemoBackend* require_memo_backend(xbase::DbArea& area, std::string* err) {
    auto* st = cli_memo::memo_backend_for(area);
    if (!st && err) *err = "Memo backend is not open for this work area.";
    return st;
}

// Backward-compat alias during transition.
inline dottalk::memo::IMemoBackend* require_memo_store(xbase::DbArea& area, std::string* err) {
    return require_memo_backend(area, err);
}

} // namespace cli_memo

// ---- Compatibility shims for existing cmd_replace.cpp -----------------------

namespace cli_memo {

struct _NoDel {
    void operator()(dottalk::memo::IMemoBackend*) const noexcept {}
};

using MemoStoreHandle = std::unique_ptr<dottalk::memo::IMemoBackend, _NoDel>;

// Legacy acquire_memo_store(...) used by current cmd_replace.cpp
inline MemoStoreHandle acquire_memo_store(
    xbase::DbArea& area,
    bool /*autoCreate*/,
    std::string* err)
{
    auto* st = cli_memo::memo_backend_for(area);
    if (!st) {
        if (err) *err = "Memo backend is not open for this work area.";
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