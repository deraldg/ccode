#pragma once
// Public entrypoint for the refactored BROWSE command.
//
// Keep this signature identical to the old command so callers don't change.
#include <sstream>
namespace xbase { class DbArea; }

namespace dottalk::browse {
void cmd_BROWSE(::xbase::DbArea& area, std::istringstream& in);
} // namespace dottalk::browse
