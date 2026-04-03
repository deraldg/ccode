// src/cli/locate_state.hpp
// Lightweight, header-only state carrier for LOCATE/CONTINUE.
// Keeps the last FOR/expression text and the next record to start from.

#pragma once
#include <string>
#include <optional>

namespace locate_state {

struct Snapshot {
    std::string where_text;        // canonical text of the last LOCATE condition
    std::optional<long> next_rec;  // recno() to start from on CONTINUE (if any)
    bool complex{false};           // whether we used the DotTalk boolean engine
};

inline Snapshot& global() {
    static Snapshot s;
    return s;
}

inline void clear() {
    global() = Snapshot{};
}

inline void set_after_match(const std::string& where, long current_recno, bool was_complex) {
    auto& s = global();
    s.where_text = where;
    s.next_rec   = current_recno + 1; // CONTINUE starts at the following record
    s.complex    = was_complex;
}

} // namespace locate_state




