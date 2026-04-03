// src/cli/cmd_browse.cpp — thin forwarder to the refactored BROWSE module

#include <sstream>
#include "xbase.hpp"

// Public entrypoint from the new module
#include "browse/browse_cmd.hpp"

// Keep the original global symbol so existing callers don't change.
void cmd_BROWSE(::xbase::DbArea& area, std::istringstream& in) {
    dottalk::browse::cmd_BROWSE(area, in);
}
