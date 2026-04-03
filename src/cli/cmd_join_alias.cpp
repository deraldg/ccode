// cmd_join_alias.cpp
// Optional convenience: allow ". JOIN ..." as an alias for ". REL JOIN ...".
//
// This file is intentionally tiny: it just forwards to the existing REL JOIN handler.
//
// Integration steps (pick the one that matches your codebase):
//   A) If you have a central command table/registry, add an entry mapping "JOIN" -> cmd_JOIN.
//   B) If your command system auto-registers cmd_* symbols, simply add this file to the build.
//
// If your project already has a JOIN command name, do NOT add this.

#include <sstream>

// forward declaration (matches signature used across cli command handlers)
namespace xbase { class DbArea; }

// Your project should already define this in cmd_relations.cpp (or similar).
void cmd_REL_JOIN(xbase::DbArea& area, std::basic_istringstream<char>& iss);

void cmd_JOIN(xbase::DbArea& area, std::basic_istringstream<char>& iss)
{
    // Forward as-is: ". JOIN <args...>" behaves like ". REL JOIN <args...>".
    cmd_REL_JOIN(area, iss);
}
