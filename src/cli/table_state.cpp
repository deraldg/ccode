#include "cli/table_state.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <iomanip>   // for std::setw, std::hex

#include "xbase.hpp"

namespace dottalk::table {

// ─────────────────────────────────────────────────────────────────────────────
// Static Data
// ─────────────────────────────────────────────────────────────────────────────

static std::array<AreaState, xbase::MAX_AREA> g_state{};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static inline bool any_stale_bits(const AreaState& s) {
    for (int i = 0; i < kWords; ++i) {
        if (s.stale_bits[i] != 0) return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// State Access
// ─────────────────────────────────────────────────────────────────────────────

bool get_state(int area0, AreaState& out) {
    if (!in_range(area0)) return false;
    out = g_state[area0];
    return true;
}

bool is_enabled(int area0) {
    return in_range(area0) && g_state[area0].enabled;
}

bool is_dirty(int area0) {
    return in_range(area0) && g_state[area0].dirty;
}

bool is_stale(int area0) {
    if (!in_range(area0)) return false;
    const auto& s = g_state[area0];
    return s.stale_any || any_stale_bits(s);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters
// ─────────────────────────────────────────────────────────────────────────────

void set_enabled(int area0, bool enabled) {
    if (!in_range(area0)) return;
    auto& s = g_state[area0];
    s.enabled = enabled;
    if (enabled) {
        s.tb.clear();
    }
}

void set_dirty(int area0, bool dirty) {
    if (!in_range(area0)) return;
    g_state[area0].dirty = dirty;
}

void set_stale(int area0, bool stale) {
    if (!in_range(area0)) return;
    auto& s = g_state[area0];
    if (stale) {
        s.stale_any = true;
    } else {
        s.stale_any = false;
        std::memset(s.stale_bits, 0, sizeof(s.stale_bits));
    }
}

void mark_stale_field(int area0, int field1) {
    if (!in_range(area0)) return;
    if (field1 <= 0 || field1 > kMaxFields) return;

    auto& s = g_state[area0];
    const int idx0 = field1 - 1;
    const int word = idx0 / 64;
    const int bit  = idx0 % 64;
    s.stale_bits[word] |= (std::uint64_t{1} << bit);
}

void clear_stale_fields(int area0) {
    if (!in_range(area0)) return;
    std::memset(g_state[area0].stale_bits, 0, sizeof(g_state[area0].stale_bits));
}

bool get_stale_fields(int area0, std::vector<int>& out_field1) {
    out_field1.clear();
    if (!in_range(area0)) return false;

    const auto& s = g_state[area0];
    for (int field1 = 1; field1 <= kMaxFields; ++field1) {
        const int idx0 = field1 - 1;
        const int word = idx0 / 64;
        const int bit  = idx0 % 64;
        if (s.stale_bits[word] & (std::uint64_t{1} << bit)) {
            out_field1.push_back(field1);
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Global Operations
// ─────────────────────────────────────────────────────────────────────────────

void set_enabled_all(bool enabled) {
    for (int i = 0; i < xbase::MAX_AREA; ++i) set_enabled(i, enabled);
}

void clear_dirty_all() {
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        g_state[i].dirty = false;
        g_state[i].tb.clear();
    }
}

void clear_stale_all() {
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        g_state[i].stale_any = false;
        std::memset(g_state[i].stale_bits, 0, sizeof(g_state[i].stale_bits));
    }
}

int count_enabled() {
    int n = 0;
    for (int i = 0; i < xbase::MAX_AREA; ++i)
        if (g_state[i].enabled) ++n;
    return n;
}

int count_dirty() {
    int n = 0;
    for (int i = 0; i < xbase::MAX_AREA; ++i)
        if (g_state[i].dirty) ++n;
    return n;
}

int count_stale() {
    int n = 0;
    for (int i = 0; i < xbase::MAX_AREA; ++i)
        if (is_stale(i)) ++n;
    return n;
}

void reset_all() {
    for (auto& state : g_state) {
        state = AreaState{};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// History Mode Control
// ─────────────────────────────────────────────────────────────────────────────

bool is_history_enabled(int area0) {
    return in_range(area0) && g_state[area0].tb.history_enabled;
}

void set_history_enabled(int area0, bool value) {
    if (!in_range(area0)) return;
    g_state[area0].tb.history_enabled = value;
}

// ─────────────────────────────────────────────────────────────────────────────
// Table Buffer Access
// ─────────────────────────────────────────────────────────────────────────────

TableBuffer& get_tb(int area0) {
    if (!in_range(area0)) throw std::out_of_range("Invalid area index");
    return g_state[area0].tb;
}

const TableBuffer& get_tb_const(int area0) {
    if (!in_range(area0)) throw std::out_of_range("Invalid area index");
    return g_state[area0].tb;
}

// ─────────────────────────────────────────────────────────────────────────────
// TableBuffer Implementation
// ─────────────────────────────────────────────────────────────────────────────

void TableBuffer::add_change(int recno, std::uint64_t flags,
                             const std::uint64_t* source_field_bits,
                             int field1, const std::string& new_value) {
    if (changes.size() >= kMaxChanges) {
        std::cout << "Warning: TableBuffer max changes reached (" << kMaxChanges << ").";
        return;
    }

    auto next_prio = [&]() -> int {
        // Priority 0 is reserved ("null"/unset). We keep 1..255 and wrap.
        if (next_priority <= 0) next_priority = 1;
        const int p = next_priority;
        ++next_priority;
        if (next_priority > 255) next_priority = 1;
        return p;
    };

    // Non-history mode: keep exactly ONE merged snapshot per recno.
    if (!history_enabled) {
        auto range = changes.equal_range(recno);

        if (range.first == range.second) {
            ChangeEntry entry;
            entry.recno       = recno;
            entry.dirty_flags = flags;
            entry.priority    = 1;

            if (source_field_bits) {
                std::memcpy(entry.field_bits, source_field_bits, sizeof(entry.field_bits));
            }

            if (field1 > 0) {
                entry.new_values[field1] = new_value;
            }

            changes.emplace(recno, std::move(entry));
            return;
        }

        // If legacy state has multiple entries, pick newest and fold others into it.
        auto newest = range.first;
        for (auto it = std::next(range.first); it != range.second; ++it) {
            if (it->second.priority > newest->second.priority) newest = it;
        }

        newest->second.dirty_flags |= flags;

        if (source_field_bits) {
            for (int i = 0; i < kWords; ++i) {
                newest->second.field_bits[i] |= source_field_bits[i];
            }
        }

        if (field1 > 0) {
            newest->second.new_values[field1] = new_value;
        }

        for (auto it = range.first; it != range.second; ) {
            if (it == newest) { ++it; continue; }

            newest->second.dirty_flags |= it->second.dirty_flags;
            for (int i = 0; i < kWords; ++i) {
                newest->second.field_bits[i] |= it->second.field_bits[i];
            }
            for (const auto& kv : it->second.new_values) {
                newest->second.new_values[kv.first] = kv.second;
            }

            it = changes.erase(it);
        }

        newest->second.priority = 1;
        return;
    }

    // History mode: append an entry per write; reads pick highest priority per field.
    ChangeEntry entry;
    entry.recno       = recno;
    entry.dirty_flags = flags;
    entry.priority    = next_prio();

    if (source_field_bits) {
        std::memcpy(entry.field_bits, source_field_bits, sizeof(entry.field_bits));
    }

    if (field1 > 0) {
        entry.new_values[field1] = new_value;
    }

    changes.emplace(recno, std::move(entry));
}


void test_add_change(int area0, int recno, std::uint64_t flags,
                     int field1, const std::string& new_value) {
    if (!in_range(area0)) {
        std::cout << "Invalid area index.\n";
        return;
    }

    auto& tb = get_tb(area0);

    std::uint64_t* field_ptr = nullptr;
    std::uint64_t temp_bits[kWords]{};

    if (field1 > 0 && field1 <= kMaxFields) {
        const int idx0 = field1 - 1;
        const int word = idx0 / 64;
        const int bit  = idx0 % 64;
        temp_bits[word] = (std::uint64_t{1} << bit);
        field_ptr = temp_bits;
    }

    std::cout << "test_add_change: area=" << area0
              << " recno=" << recno
              << " field=" << field1
              << " value=\"" << new_value << "\"\n";

    tb.add_change(recno, flags, field_ptr, field1, new_value);

    std::cout << " → Change added (history " << (tb.history_enabled ? "ON" : "OFF") << ")\n";
}

} // namespace dottalk::table