#pragma once
// dli/set_view.hpp — stub of SET VIEW command
// Declares the dli::cmd_SET_VIEW entrypoint used by dli/cmd_set.cpp.
//
// This is a minimal placeholder to keep the build green until
// a real View subsystem is implemented.

#include <sstream>

namespace xbase { class DbArea; }

namespace dli {
void cmd_SET_VIEW(xbase::DbArea& db, std::istringstream& args);
} // namespace dli
