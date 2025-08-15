#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <sstream>
#include "xbase.hpp"

namespace cli {

using Handler = std::function<void(xbase::DbArea&, std::istringstream&)>;

class CommandRegistry {
public:
    void add(std::string name, Handler h);
    bool run(const std::string& name, xbase::DbArea& area, std::istringstream& iss) const;
    void help(std::ostream& os) const;
private:
    std::unordered_map<std::string, Handler> map_;
};

} // namespace cli
