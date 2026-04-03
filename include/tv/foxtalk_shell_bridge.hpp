#pragma once

#include <cstddef>
#include <sstream>
#include <string>

#include "xbase.hpp"

namespace foxtalk {

struct ShellRunResult {
    bool handled{false};
    bool success{false};
    std::string statusMessage;
};

class ShellBridge {
public:
    ShellBridge() = default;

    std::string resolveShortcuts(const std::string& line) const;

    ShellRunResult runLine(xbase::DbArea& area, const std::string& line);

private:
    ShellRunResult runBuiltIn(xbase::DbArea& area,
                              const std::string& resolvedLine,
                              const std::string& commandUpper,
                              std::istringstream& tok);

    ShellRunResult runEngineCommand(xbase::DbArea& area,
                                    const std::string& commandUpper,
                                    std::istringstream& tok,
                                    const std::string& originalCommandToken,
                                    const std::string& resolvedLine);

    void handleHelp(std::istringstream& tok);
    void doSelfTest(std::size_t n);
};

} // namespace foxtalk