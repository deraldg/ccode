#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <sstream>

#include "xbase.hpp"   // for xbase::DbArea

// ===== DotTalk Line Interface (dli) =========================================
//  - Public API is unchanged except the namespace is now dli:: instead of the old namespace
//  - A temporary alias `namespace cli = dli;` is provided at the bottom so
//    existing call sites continue to compile during the rename sweep.

namespace dli {

// Handlers take (DbArea&, args-stream)
using Handler = std::function<void(xbase::DbArea&, std::istringstream&)>;

struct CommandRegistry {
  // Register or replace a command handler by exact, already-normalized name.
  void add(const std::string& name, Handler h);

  // Dispatch a command; returns true to keep shell alive (normal case).
  bool run(xbase::DbArea& area,
           const std::string& normalized_key,
           std::istringstream& args);

  // Access the internal map (read-only)
  const std::unordered_map<std::string, Handler>& map() const { return map_; }

private:
  std::unordered_map<std::string, Handler> map_;
};

// Global singleton accessor (defined in the .cpp)
CommandRegistry& registry();

// Convenience helpers that some code references directly.
void register_command(const std::string& name, dli::Handler h);
const std::unordered_map<std::string, dli::Handler>& map();

} // namespace dli

// ---------------------------------------------------------------------------
// TEMPORARY BACK-COMPAT ALIAS:
//   Keep this until you finish sweeping \bcli:: -> dli:: across the repo.
//   Afterwards, remove these two lines and rebuild to catch any stragglers.


