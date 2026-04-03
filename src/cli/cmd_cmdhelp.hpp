// ============================================================================
// File: src/cli/cmd_cmdhelp.hpp
// ============================================================================
#pragma once
#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>

#include "command_registry.hpp" // dli::map()
#include "foxref.hpp"           // foxref::catalog()
#include "dotref.hpp"           // dotref::catalog()
namespace xbase { class DbArea; }

namespace cmdhelp {

struct CommandInfo {
    int         id{0};
    std::string name;               // UPPER
    bool        implemented{false};
    std::string catalog;            // FOX | DOT
    bool        supported{false};   // catalog says supported
    std::string usage;              // from catalog syntax
    std::string verbose;            // from catalog summary
};

struct ArgInfo {
    int         id{0};
    std::string catalog;            // parent catalog (UPPER)
    std::string command;            // parent command (UPPER)
    std::string arg;                // switch (UPPER)
    std::string usage;              // usage examples/syntax
    std::string verbose;            // explanation
};

// Registry ? foxref/dotref
std::vector<CommandInfo> collect_commands();

// FoxRef syntax ? mined switches from source tree (SINGLE SIGNATURE)
std::vector<ArgInfo> collect_args(const std::vector<CommandInfo>& cmds,
                                  const std::vector<std::string>& source_roots);

// Pretty report
void print_commands_report(std::ostream& os, const std::vector<CommandInfo>& cmds);

// Emit commands.dbf + cmd_args.dbf (SINGLE SIGNATURE)
struct DbfWriteCounts { int commands{0}; int args{0}; };
DbfWriteCounts export_dbfs(const std::string& out_dir,
                           const std::vector<std::string>& source_roots);

// CLI entrypoint (wire this). Free-function bridge is in the .cpp.
void cmd_COMMANDSHELP(xbase::DbArea& area, std::istringstream& in);

} // namespace cmdhelp



