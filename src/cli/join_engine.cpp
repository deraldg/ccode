// File: src/cli/join_engine.cpp
//
// Implements the REL JOIN execution hook used by the CLI command layer.
//
// After the "true join enumerator" change, REL JOIN delegates to
// relations_api::enum_emit_for_current_parent() (same behavior as REL ENUM).
//
// The historical single-row behavior is preserved as REL JOIN ONE, implemented by
// join_emit_one_for_current_parent() below.

#include "set_relations.hpp"
#include "workareas.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace relations_api {

// Provided by shell.cpp (C linkage there)
extern "C" xbase::XBaseEngine* shell_engine();

namespace {

class WorkAreaCursorRestore {
public:
    WorkAreaCursorRestore()
        : eng_(shell_engine())
        , start_area_(eng_ ? eng_->currentArea() : -1) {
        const std::size_t n = workareas::count();
        saved_.reserve(n);

        for (std::size_t i = 0; i < n; ++i) {
            xbase::DbArea* a = workareas::at(i);
            if (!a) continue;

            bool open = false;
            try { open = a->isOpen(); } catch (...) { open = false; }
            if (!open) continue;

            int32_t rn = 0;
            try { rn = a->recno(); } catch (...) { rn = 0; }

            saved_.push_back(Saved{static_cast<int>(i), rn});
        }
    }

    ~WorkAreaCursorRestore() {
        if (!eng_) return;

        const std::size_t n = workareas::count();

        for (const auto& s : saved_) {
            if (s.slot < 0) continue;
            const std::size_t slot = static_cast<std::size_t>(s.slot);
            if (slot >= n) continue;

            xbase::DbArea* a = workareas::at(slot);
            if (!a) continue;

            bool open = false;
            try { open = a->isOpen(); } catch (...) { open = false; }
            if (!open) continue;

            const int32_t rn = s.recno;
            const int32_t rc = [&]() {
                try { return a->recCount(); } catch (...) { return 0; }
            }();

            if (rn >= 1 && rn <= rc) {
                try { a->gotoRec(rn); } catch (...) {}
                try { a->readCurrent(); } catch (...) {}
            } else if (rc > 0) {
                try { a->top(); } catch (...) {}
                try { a->readCurrent(); } catch (...) {}
            }
        }

        if (start_area_ >= 0) {
            try { eng_->selectArea(start_area_); } catch (...) {}
        }

        // Refresh any cached relation pointers that depend on current records.
        relations_api::refresh_if_enabled();
    }

private:
    struct Saved { int slot; int32_t recno; };
    xbase::XBaseEngine* eng_{nullptr};
    int start_area_{-1};
    std::vector<Saved> saved_;
};

} // namespace

bool join_emit_one_for_current_parent(
    const std::vector<std::string>& /*child_chain*/,
    std::size_t max_rows,
    const std::function<void()>& emit,
    std::size_t* out_rows) {
    if (out_rows) *out_rows = 0;

    WorkAreaCursorRestore preserve;

    // Treat max_rows == 0 as "unlimited".
    if (max_rows != 0 && max_rows < 1) return true;

    emit();
    if (out_rows) *out_rows = 1;
    return true;
}

bool join_emit_for_current_parent(
    const std::vector<std::string>& child_chain,
    std::size_t max_rows,
    const std::function<void()>& emit,
    std::size_t* out_rows) {
    // Delegate to enumerator: same semantics as REL ENUM (with LIMIT).
    return enum_emit_for_current_parent(child_chain, max_rows, emit, out_rows);
}

} // namespace relations_api
