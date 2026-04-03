#include "status_helpers.hpp"
#include "cli/order_state.hpp"
#include <sstream>
#include <cctype>
#include <string>

namespace {
inline char upc(char c){ return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
inline bool ends_with_3(const std::string& s, const char* ext3) {
    const size_t n = s.size();
    return n >= 4 && s[n-4]=='.' && upc(s[n-3])==ext3[0] && upc(s[n-2])==ext3[1] && upc(s[n-1])==ext3[2];
}
} // anon

namespace status {

std::string format_active_order(const xbase::DbArea& A) {
    std::ostringstream os;
    if (!orderstate::hasOrder(A)) {
        os << "Order       : PHYSICAL";
        return os.str();
    }

    const std::string idxName = orderstate::orderName(A);
    const bool asc = orderstate::isAscending(A);
    os << "Order       : " << (asc ? "ASCEND" : "DESCEND");

    if (!idxName.empty()) {
        os << "\n  Index file  : " << idxName;

        // Only CNX carries a tag concept here
        if (ends_with_3(idxName, "CNX")) {
            const std::string tag = orderstate::activeTag(A);
            os << "\n  Active tag  : " << (tag.empty() ? "(none)" : tag);
        } else {
            os << "\n  Active tag  : (none)";
        }
    } else {
        os << "\n  Index file  : (none)"
           << "\n  Active tag  : (none)";
    }

    return os.str();
}

} // namespace status



