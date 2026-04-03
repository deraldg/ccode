#pragma once
// include/cli/order_state.hpp
// Per-area, CLI-visible order state.
//
// Notes:
// - This is intentionally lightweight: it does NOT own index backends.
// - It only tracks what the CLI considers the “active order container” and
//   (for tag containers) the active tag name.

#include <string>

namespace xbase { class DbArea; }

namespace orderstate {

// Sets/clears active order container path (e.g., "<name>.inx", "<name>.cnx", "<name>.cdx").
// Opening one index implicitly closes the other for the same area.
void setOrder(xbase::DbArea& area, const std::string& container_path);
void clearOrder(xbase::DbArea& area);

// Query active order.
bool hasOrder(const xbase::DbArea& area);
std::string orderName(const xbase::DbArea& area);

// Direction helpers (default: ASCEND).
void setAscending(xbase::DbArea& area, bool ascending);
bool isAscending(const xbase::DbArea& area);

// Tag helpers for tag-container index formats (CNX, CDX).
// For non-tag formats (INX/ISX/CSX), the stored tag is cleared.
void setActiveTag(xbase::DbArea& area, const std::string& tag_name);
std::string activeTag(const xbase::DbArea& area);

// Container-type helpers.
// These classify the currently active order container by suffix.
bool isInx(const xbase::DbArea& area);
bool isCnx(const xbase::DbArea& area);
bool isCdx(const xbase::DbArea& area);
bool isIsx(const xbase::DbArea& area);
bool isCsx(const xbase::DbArea& area);

} // namespace orderstate