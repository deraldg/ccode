
// src/cli/order_display.hpp
#pragma once
#include <string>

namespace xbase { class DbArea; }

namespace orderdisplay {

// Returns a single-line summary for AREA/LIST headers, e.g.:
//   "Order: ASCEND  File: students.inx"
//   "Order: DESCEND File: students.cnx  CNX TAG: LNAME"
std::string summarize(const xbase::DbArea& area);

} // namespace orderdisplay



