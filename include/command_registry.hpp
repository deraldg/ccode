#pragma once

#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace xbase { class DbArea; }

namespace cli {

using Handler = std::function<void(xbase::DbArea&, std::istringstream&)>;

class CommandRegistry {
public:
  // Register or replace a command handler by name (case-insensitive).
  void add(const std::string& name, Handler h);

  // Look up and run a command. Returns false if not found.
  bool run(xbase::DbArea& area, const std::string& name, std::istringstream& args) const;

  // Returns the list of registered (uppercased) command names.
  const std::vector<std::string>& list() const;
};

// Global access point (function-local static, safe in MSVC/C++11+).
CommandRegistry& registry();

} // namespace cli

// ------- Backward-compatibility shims (if older code still calls these) -----
namespace command_registry {
  void register_command(const std::string& name, cli::Handler h);
  const std::unordered_map<std::string, cli::Handler>& map();
  const std::vector<std::string>& list_names();
} // namespace command_registry
