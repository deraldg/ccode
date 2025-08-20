#include "order_state.hpp"
#include "xbase.hpp"
#include <unordered_map>
#include <string>

namespace {
    struct State {
        bool has_order = false;
        std::string path;   // stored path or tag name
        bool ascending = true;
    };
    // Track state per area pointer
    std::unordered_map<const xbase::DbArea*, State> g;
}

namespace orderstate {

bool hasOrder(const xbase::DbArea& a) {
    auto it = g.find(&a);
    return (it != g.end()) && it->second.has_order;
}

std::string orderName(const xbase::DbArea& a) {
    auto it = g.find(&a);
    return (it != g.end() && it->second.has_order) ? it->second.path : std::string{};
}

void setOrder(xbase::DbArea& a, const std::string& path) {
    auto& st = g[&a];
    st.has_order = true;
    st.path = path;
    // keep current ascending flag as-is
}

void clearOrder(xbase::DbArea& a) {
    auto& st = g[&a];
    st.has_order = false;
    st.path.clear();
    st.ascending = true;
}

void setAscending(xbase::DbArea& a, bool asc) {
    g[&a].ascending = asc;
}

bool isAscending(const xbase::DbArea& a) {
    auto it = g.find(&a);
    return (it == g.end()) ? true : it->second.ascending;
}

} // namespace orderstate
