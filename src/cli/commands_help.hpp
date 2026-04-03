// ============================================================================
// /src/cli/commands_help.hpp  (UPDATED)
// ============================================================================

#pragma once
#include <iosfwd>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

#include "command_registry.hpp" // dli::map()
#include "foxref.hpp"           // foxref::catalog()

namespace xbase { class DbArea; }

namespace cmdhelp {

struct CommandInfo {
    int         id{0};
    std::string name;               // canonical UPPER
    std::string source;             // "Homegrown", "FoxPro", "PowerShell"
    bool        implemented{false};
    bool        foxref_supported{false};
    std::string usage;              // from foxref.syntax or pshell.syntax
    std::string verbose;            // from foxref.summary or pshell.summary
};

struct ArgInfo {
    int         id{0};
    std::string command;            // UPPER
    std::string arg;                // UPPER switch token
    std::string usage;              // usage examples (seeded)
    std::string verbose;            // explanation (seeded)
};

// Build the in-memory catalog by merging registry + foxref + pshell
std::vector<CommandInfo> collect_commands();

// Derive arguments (switches) from the syntax in foxref & source scanning
std::vector<ArgInfo> collect_args(const std::vector<CommandInfo>& cmds,
                                  const std::vector<std::string>& source_roots = { "./src" });

// Pretty report to stdout (now grouped by source)
void print_commands_report(std::ostream& os,
                           const std::vector<CommandInfo>& cmds);

// Write commands.dbf and cmd_args.dbf (C/L/N only for now)
struct DbfWriteCounts { int commands{0}; int args{0}; };
DbfWriteCounts export_dbfs(const std::string& out_dir = ".",
                           const std::vector<std::string>& source_roots = { "./src" });

// CLI entrypoint: COMMANDSHELP [outdir] [src_root?]
void cmd_COMMANDSHELP(xbase::DbArea& area, std::istringstream& in);

} // namespace cmdhelp