// include/cli/table_object.hpp
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "cli/table_state.hpp"

namespace dottalk::table {

// Identifies the provenance of a single value cell.
// This is what makes overlay deterministic for single-area rows and joined tuples.
struct FieldRef {
    int area0  = -1;
    int recno  = -1;
    int field1 =  0;  // 1-based field index
};

// A minimal "meta-record" suitable for tuple/display/browse use.
// - values: the cell values (usually strings for now; typed later if desired)
// - refs  : per-cell provenance (same length as values)
// - flags : row intent flags (CHANGE_UPDATE/DELETE/INSERT) aggregated from buffer
struct Row {
    int area0 = -1;
    int recno = -1;

    std::uint64_t flags = 0;

    std::vector<std::string> values;
    std::vector<FieldRef>    refs;   // optional, but strongly recommended (esp. for joins)
};

// Overlay patch for a single (area0, recno).
// new_values maps field1 -> buffered string value.
struct Overlay {
    bool has = false;

    std::uint64_t flags = 0;               // OR of ChangeType bits across entries
    int           best_priority = -1;       // informational; highest priority seen

    std::map<int, std::string> new_values;  // field1 -> new value
};

// TABLE object: workspace facade over one DBF work area.
// - Provides physical snapshot of a record
// - Provides overlay patch (buffered edits) for (area0, recno)
// - Applies overlay to rows/tuples so user sees edits without COMMIT
class Table {
public:
    Table(xbase::XBaseEngine& eng, int area0);

    int area_index() const noexcept { return _area0; }

    xbase::DbArea&       area();
    const xbase::DbArea& area() const;

    bool is_open() const noexcept;
    bool table_enabled() const;

    // Return the merged overlay patch for (area0, recno).
    // If multiple ChangeEntry exist for recno, later/higher priority wins per-field.
    Overlay overlay_for(int recno) const;

    // Overlay one scalar value if a buffered change exists for (area0, recno, field1).
    // Returns true if it overrode the value.
    bool overlay_value(int recno, int field1, std::string& inout) const;

    // Applies overlay to a Row using per-cell provenance (Row.refs).
    // If Row.refs is empty, this will treat it as a single-area row in this Table:
    //   values[i] corresponds to field1 = i+1 at (this area0, row.recno).
    void apply_overlay(Row& row) const;

    // Snapshot helpers for the common single-area case
    // (all fields from this table/area).
    Row snapshot_physical(int recno) const;
    Row snapshot_view(int recno) const; // physical + overlay if TABLE ON

private:
    xbase::XBaseEngine& _eng;
    int                _area0 = 0;

    // Reads all field values at the given recno without permanently moving the cursor.
    // Throws on open/recno failures (caller decides how to report).
    std::vector<std::string> read_record_values_no_side_effect(int recno) const;

    // Merge overlay entries for a specific recno from the TableBuffer.
    Overlay merge_overlay_from_tb(int recno, const TableBuffer& tb) const;
};

} // namespace dottalk::table
