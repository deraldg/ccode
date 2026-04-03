#pragma once

#include <cstdint>

namespace xbase { class DbArea; }

namespace cli::logical_nav {

// True if the given record is visible in the current logical view
// (active order + SET FILTER + SET DELETED policy as enforced by filter::visible()).
bool is_visible(xbase::DbArea& area, int32_t recno);

// First / last visible record in the current logical order.
// Returns 0 if no visible record exists.
int32_t first_recno(xbase::DbArea& area);
int32_t last_recno(xbase::DbArea& area);

// Next / previous visible record in the current logical order.
// from_recno == 0 means "start from endpoint":
//   next_recno(..., 0) -> first visible
//   prev_recno(..., 0) -> last visible
//
// Returns 0 if no matching next/previous visible record exists.
int32_t next_recno(xbase::DbArea& area, int32_t from_recno);
int32_t prev_recno(xbase::DbArea& area, int32_t from_recno);

} // namespace cli::logical_nav
