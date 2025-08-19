#pragma once
#include <functional>
#include <iosfwd>
#include <map>
#include <sstream>
#include <string>

namespace xbase { class DbArea; }

namespace cli {

using Handler = std::function<void(xbase::DbArea&, std::istringstream&)>;

class CommandRegistry {
public:
  void add(const std::string& name, Handler h);

  // Parse a whole input line: splits out the command and passes the rest as args
  bool run(xbase::DbArea& area, const std::string& line);

  // Direct command + already-prepared args stream
  bool run(xbase::DbArea& area, const std::string& cmd, std::istringstream& args);

  void help(std::ostream& os) const;

private:
  std::map<std::string, Handler> map_;
};

// Global registry used by the CLI
extern CommandRegistry reg;

} // namespace cli
