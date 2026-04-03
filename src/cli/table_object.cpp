// src/cli/table_object.cpp
#include "cli/table_object.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>

namespace dottalk::table {

Table::Table(xbase::XBaseEngine& eng, int area0)
    : _eng(eng), _area0(area0) {
    if (!in_range(area0)) {
        throw std::out_of_range("Table: area0 out of range");
    }
}

xbase::DbArea& Table::area() {
    return _eng.area(_area0);
}

const xbase::DbArea& Table::area() const {
    return _eng.area(_area0);
}

bool Table::is_open() const noexcept {
    try {
        return _eng.area(_area0).isOpen();
    } catch (...) {
        return false;
    }
}

bool Table::table_enabled() const {
    return dottalk::table::is_enabled(_area0);
}

Overlay Table::merge_overlay_from_tb(int recno, const TableBuffer& tb) const {
    Overlay out{};

    if (recno <= 0) return out;

    auto range = tb.changes.equal_range(recno);
    if (range.first == range.second) return out;

    // Merge strategy:
    // - Aggregate flags by OR
    // - For per-field values, apply entries in priority order (ascending),
    //   so the highest priority ends up winning. If priority ties, insertion
    //   order within the multimap range is acceptable for now.
    struct EntryPtr {
        const ChangeEntry* e;
    };
    std::vector<const ChangeEntry*> entries;
    for (auto it = range.first; it != range.second; ++it) {
        entries.push_back(&it->second);
    }

    std::stable_sort(entries.begin(), entries.end(),
        [](const ChangeEntry* a, const ChangeEntry* b) {
            return a->priority < b->priority;
        });

    out.has = true;
    for (const ChangeEntry* e : entries) {
        out.flags |= e->dirty_flags;
        out.best_priority = std::max(out.best_priority, e->priority);

        // last-wins (by priority ordering)
        for (const auto& kv : e->new_values) {
            out.new_values[kv.first] = kv.second;
        }
    }

    return out;
}

Overlay Table::overlay_for(int recno) const {
    if (!in_range(_area0)) return Overlay{};

    // Even if table_enabled() is false, callers may still want to inspect.
    // But "apply_overlay" will only act when enabled.
    const auto& tb = dottalk::table::get_tb_const(_area0);
    return merge_overlay_from_tb(recno, tb);
}

bool Table::overlay_value(int recno, int field1, std::string& inout) const {
    if (!table_enabled()) return false;
    if (recno <= 0 || field1 <= 0) return false;

    const auto ov = overlay_for(recno);
    if (!ov.has) return false;

    auto it = ov.new_values.find(field1);
    if (it == ov.new_values.end()) return false;

    inout = it->second;
    return true;
}

void Table::apply_overlay(Row& row) const {
    if (!table_enabled()) return;

    // Determine which area/recno to use per cell.
    // If refs are not provided, assume single-area row for this table.
    if (row.recno <= 0) return;

    // If caller didn't set area0, default it to this table's area.
    if (row.area0 < 0) row.area0 = _area0;

    if (!row.refs.empty() && row.refs.size() != row.values.size()) {
        throw std::runtime_error("Table::apply_overlay: refs size mismatch");
    }

    // We may need multiple overlays if this row is a join tuple across areas.
    // Cache overlays per (area0, recno) for this one apply call.
    struct Key {
        int area0;
        int recno;
        bool operator<(const Key& o) const {
            if (area0 != o.area0) return area0 < o.area0;
            return recno < o.recno;
        }
    };
    std::map<Key, Overlay> ov_cache;

    auto get_overlay_cached = [&](int area0, int recno) -> const Overlay& {
        Key k{area0, recno};
        auto it = ov_cache.find(k);
        if (it != ov_cache.end()) return it->second;

        // If overlay references another area, consult table_state for that area.
        const auto& tb = dottalk::table::get_tb_const(area0);
        Overlay ov = merge_overlay_from_tb(recno, tb);
        auto [ins_it, _] = ov_cache.emplace(k, std::move(ov));
        return ins_it->second;
    };

    // Apply overlay per value cell
    for (size_t i = 0; i < row.values.size(); ++i) {
        int a0   = _area0;
        int rno  = row.recno;
        int f1   = static_cast<int>(i) + 1;

        if (!row.refs.empty()) {
            a0  = row.refs[i].area0;
            rno = row.refs[i].recno;
            f1  = row.refs[i].field1;
        }

        if (!in_range(a0) || rno <= 0 || f1 <= 0) continue;
        if (!dottalk::table::is_enabled(a0)) continue;

        const Overlay& ov = get_overlay_cached(a0, rno);
        if (!ov.has) continue;

        auto it = ov.new_values.find(f1);
        if (it != ov.new_values.end()) {
            row.values[i] = it->second;
            row.flags |= ov.flags;
        }
    }
}

std::vector<std::string> Table::read_record_values_no_side_effect(int recno) const {
    auto& db = _eng.area(_area0);
    if (!db.isOpen()) throw std::runtime_error("Table: area not open");
    if (recno <= 0 || recno > db.recCount()) throw std::runtime_error("Table: recno out of range");

    const int saved = db.recno();

    // Move, read, extract, restore.
    if (!db.gotoRec(recno)) throw std::runtime_error("Table: gotoRec failed");
    (void)db.readCurrent(); // best-effort; many callers already keep current loaded

    const int n = db.fieldCount();
    std::vector<std::string> vals;
    vals.reserve(static_cast<size_t>(n));
    for (int f1 = 1; f1 <= n; ++f1) {
        vals.push_back(db.get(f1));
    }

    // Restore cursor if possible (avoid messing with calling command)
    if (saved > 0 && saved <= db.recCount()) {
        (void)db.gotoRec(saved);
        (void)db.readCurrent();
    }

    return vals;
}

Row Table::snapshot_physical(int recno) const {
    Row row{};
    row.area0 = _area0;
    row.recno = recno;

    row.values = read_record_values_no_side_effect(recno);

    // Populate provenance for the single-area case (optional but useful)
    row.refs.reserve(row.values.size());
    for (size_t i = 0; i < row.values.size(); ++i) {
        FieldRef fr;
        fr.area0  = _area0;
        fr.recno  = recno;
        fr.field1 = static_cast<int>(i) + 1;
        row.refs.push_back(fr);
    }

    return row;
}

Row Table::snapshot_view(int recno) const {
    Row row = snapshot_physical(recno);
    apply_overlay(row);
    return row;
}

} // namespace dottalk::table
