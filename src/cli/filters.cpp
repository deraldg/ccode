// src/cli/filters.cpp
#include "filters.hpp"
#include "xbase.hpp"
#include <unordered_map>

namespace {
    struct PtrHash { size_t operator()(const void* p) const noexcept { return std::hash<std::uintptr_t>{}(reinterpret_cast<std::uintptr_t>(p)); } };
    struct PtrEq   { bool operator()(const void* a, const void* b) const noexcept { return a==b; } };
    std::unordered_map<const void*, filters::Simple, PtrHash, PtrEq> g_areaFilters;
}

namespace filters {

void set(xbase::DbArea& area, std::optional<Simple> f) {
    const void* key = static_cast<const void*>(&area);
    if (f && f->active) {
        g_areaFilters[key] = *f;
    } else {
        g_areaFilters.erase(key);
    }
}

std::optional<Simple> get(const xbase::DbArea& area) {
    const void* key = static_cast<const void*>(&area);
    auto it = g_areaFilters.find(key);
    if (it == g_areaFilters.end()) return std::nullopt;
    return it->second;
}

} // namespace filters


