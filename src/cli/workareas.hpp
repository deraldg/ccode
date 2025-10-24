#pragma once
#include <cstddef>
#include "xbase.hpp"

// Adapter to walk DB areas using 0-based slots (Area 0..N-1).
namespace workareas {
    // Total slots (0..count()-1 are valid). Returns 0 if engine is unavailable.
    std::size_t count();

    // 0-based; returns nullptr if engine missing or slot out of range.
    xbase::DbArea* at(std::size_t slot0);

    // Optional label for the slot; prefers DbArea::name() then ::filename().
    const char* name(std::size_t slot0);

    // Current slot (0-based). Returns count() if unknown.
    std::size_t current_slot();
} // namespace workareas
