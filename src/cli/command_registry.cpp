// src/cli/command_registry.cpp
#include "command_registry.hpp"

#include <iostream>
#include <mutex>
#include <algorithm>
#include <cctype>

using xbase::DbArea;

namespace dli {

using Map = std::unordered_map<std::string, Handler>;

// Small helper: trim leading spaces from the args stream
static inline void ltrim_stream(std::istringstream& iss) {
  while (iss && std::isspace(static_cast<unsigned char>(iss.peek()))) iss.get();
}

void CommandRegistry::add(const std::string& name, Handler h) {
  map_[name] = std::move(h);
}

bool CommandRegistry::run(DbArea& area,
                          const std::string& normalized_key,
                          std::istringstream& args)
{
  auto it = map_.find(normalized_key);
  if (it == map_.end()) {
    std::cout << "Unknown command: " << normalized_key << "\n";
    return true; // keep shell alive
  }

  ltrim_stream(args);
  it->second(area, args);
  return true;
}

// Singleton instance
static CommandRegistry& instance() {
  static CommandRegistry reg;
  return reg;
}

CommandRegistry& registry() {
  return instance();
}

// Convenience wrappers
void register_command(const std::string& name, dli::Handler h) {
  registry().add(name, std::move(h));
}

const std::unordered_map<std::string, dli::Handler>& map() {
  return registry().map();
}

} // namespace dli