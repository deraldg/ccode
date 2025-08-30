#include "xindex/attach.hpp"
#include "xindex/index_manager.hpp" // ensure IndexManager is visible
#include "xbase.hpp"                // ensure xbase::DbArea is visible

#include <unordered_map>
#include <memory>

namespace xindex {

IndexManager& ensure_manager(xbase::DbArea& area) {
    // one manager per DbArea address
    static std::unordered_map<const xbase::DbArea*, std::unique_ptr<IndexManager>> map;

    auto it = map.find(&area);
    if (it == map.end()) {
        auto mgr = std::make_unique<IndexManager>(area);
        auto [ins, ok] = map.emplace(&area, std::move(mgr));
        return *ins->second;
    }
    return *it->second;
}

} // namespace xindex
