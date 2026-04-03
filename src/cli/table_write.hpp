// src/cli/table_write.hpp
#pragma once

#include "xbase.hpp"
#include "cli/table_state.hpp"
#include "xbase_locks.hpp"  // for locking

namespace dottalk {

/**
 * Centralized function to write a field value.
 * - If TABLE is enabled for the area → buffers the change (add_change).
 * - Otherwise → writes directly to DBF (set + writeCurrent).
 * 
 * Handles:
 * - Record locking
 * - Dirty/stale marking
 * - Logging (buffered or direct)
 * 
 * @param area       The DbArea to write to
 * @param field1     1-based field index
 * @param new_value  The value to write
 * @param reason     Optional string for logging (e.g. "REPLACE FNAME")
 * @return true if the write was successful (buffered or direct)
 */
bool write_field(xbase::DbArea& area, int field1, const std::string& new_value,
                 const std::string& reason = "") {
    if (!area.isOpen()) {
        std::cout << "write_field: area not open.\n";
        return false;
    }

    int area0 = resolve_current_index(area);  // reuse your existing helper
    if (area0 < 0) {
        std::cout << "write_field: cannot determine area index.\n";
        return false;
    }

    const uint32_t rn = area.recno();
    if (rn == 0) {
        std::cout << "write_field: no current record.\n";
        return false;
    }

    std::string lock_err;
    if (!xbase::locks::try_lock_record(area, rn, &lock_err)) {
        std::cout << "write_field: record locked (" << lock_err << ").\n";
        return false;
    }

    bool success = false;

    if (dottalk::table::is_enabled(area0)) {
        // Buffer mode
        auto& tb = dottalk::table::get_tb(area0);

        // Build field mask for stale tracking
        std::uint64_t field_mask[dottalk::table::kWords]{};
        int idx0 = field1 - 1;
        int word = idx0 / 64;
        int bit  = idx0 % 64;
        field_mask[word] |= (std::uint64_t{1} << bit);

        tb.add_change(static_cast<int>(rn), dottalk::table::CHANGE_UPDATE,
                      field_mask, field1, new_value);

        dottalk::table::set_dirty(area0, true);
        dottalk::table::mark_stale_field(area0, field1);

        success = true;
        if (!reason.empty()) {
            std::cout << "Buffered " << reason << " at rec " << rn << ".\n";
        }
    } else {
        // Direct write mode
        try {
            success = area.set(field1, new_value) && area.writeCurrent();
            if (success && !reason.empty()) {
                std::cout << "Direct write " << reason << " at rec " << rn << ".\n";
            }
        } catch (...) {
            success = false;
        }
    }

    xbase::locks::unlock_record(area, rn);

    if (!success) {
        std::cout << "write_field failed for rec " << rn << ".\n";
    }

    return success;
}

} // namespace dottalk