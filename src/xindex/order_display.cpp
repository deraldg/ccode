
// src/cli/order_display.cpp
#include "xindex/order_display.hpp"
#include "cli/cli_bridge.hpp"
#include <sstream>

namespace orderdisplay {

std::string summarize(const xbase::DbArea& area) {
    std::ostringstream os;
    const bool attached = xindex_cli::db_index_attached(area);
    const bool asc      = xindex_cli::db_order_asc(area);
    os << "Order: " << (asc ? "ASCEND" : "DESCEND");
    if (!attached) {
        os << "  (no index)";
        return os.str();
    }
    const std::string path = xindex_cli::db_index_path(area);
    os << "  File: " << path;
    // If CNX: show tag
    if (path.size() >= 4) {
        auto suf = path.substr(path.size()-4);
        for (auto& c : suf) c = (char)toupper((unsigned char)c);
        if (suf == ".CNX") {
            const std::string tag = xindex_cli::db_active_cnx_tag(area);
            if (!tag.empty()) {
                os << "  CNX TAG: " << tag;
            } else {
                os << "  CNX TAG: (none)";
            }
        }
    }
    return os.str();
}

} // namespace orderdisplay



