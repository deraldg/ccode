#include "../../include/command_registry.hpp"
#include <iostream>

namespace cli {

CommandRegistry reg;

void CommandRegistry::add(const std::string& name, Handler h) {
  map_[name] = std::move(h);
}

bool CommandRegistry::run(xbase::DbArea& area, const std::string& line) {
  std::istringstream iss(line);
  std::string cmd;
  if (!(iss >> cmd)) return true; // empty line: treat as handled
  return run(area, cmd, iss);
}

bool CommandRegistry::run(xbase::DbArea& area, const std::string& cmd, std::istringstream& args) {
  auto it = map_.find(cmd);
  if (it == map_.end()) return false;
  it->second(area, args);
  return true;
}

void CommandRegistry::help(std::ostream& os) const {
  for (const auto& kv : map_) os << kv.first << '\n';
}

} // namespace cli
