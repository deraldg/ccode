#pragma once
#include <string>

namespace xbase { class DbArea; }

namespace orderstate {

// Does this area currently have an order (index) attached?
bool hasOrder(const xbase::DbArea& a);

// User-facing name/path of the current order, or empty if none.
std::string orderName(const xbase::DbArea& a);

// Attach an order by path (e.g., "students.inx").
void setOrder(xbase::DbArea& a, const std::string& path);

// Clear order for this area (fall back to natural/physical order).
void clearOrder(xbase::DbArea& a);

// Ascending/descending flag for current order (default true).
void setAscending(xbase::DbArea& a, bool asc);
bool isAscending(const xbase::DbArea& a);

} // namespace orderstate
