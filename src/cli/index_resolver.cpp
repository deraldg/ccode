// src/cli/index_resolver.cpp

#include "index_resolver.hpp"
#include "xbase.hpp"

#include <string>
#include <algorithm>
#include <cctype>

// If you have a central “order state” accessor, include it here and fill the struct accordingly.
// For now this is a NATURAL fallback so area_banner compiles and runs.

namespace dottalk {

[[maybe_unused]] static std::string to_rel_path(const std::string& abs, const std::string& data_root_abs)
{
    if (abs.empty() || data_root_abs.empty()) return abs;

    // crude but safe: case-insensitive prefix strip on Windows paths
    std::string a = abs;
    std::string d = data_root_abs;

    std::transform(a.begin(), a.end(), a.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(d.begin(), d.end(), d.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (a.rfind(d, 0) == 0) {
        // abs starts with data_root_abs (case-insensitive). Slice using original strings.
        size_t cut = data_root_abs.size();

        // If abs has a separator immediately after the root, skip it.
        if (cut < abs.size()) {
            const char ch = abs[cut];
            if (ch == '\\' || ch == '/') ++cut;
        }

        if (cut <= abs.size()) return abs.substr(cut);
    }

    return abs;
}

OrderInfo query_active_order(const xbase::DbArea& /*area*/,
                             const std::string& /*data_dir_abs*/)
{
    OrderInfo oi;            // defaults to inactive/NATURAL
    // TODO: When you expose engine order state, populate:
    // oi.active       = true/false;
    // oi.tag          = current tag name if CNX/INX, else "" for IDX/unnamed;
    // oi.direction    = "ASC" or "DESC";
    // oi.kind         = "CNX" | "INX" | "IDX";
    // oi.container_rel= to_rel_path(container_abs, data_dir_abs);
    return oi;
}

} // namespace dottalk
