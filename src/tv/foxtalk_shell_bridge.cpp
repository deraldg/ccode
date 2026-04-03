#include "tv/foxtalk_shell_bridge.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "command_registry.hpp"
#include "tv/foxtalk_util.hpp"

namespace foxtalk {

std::string ShellBridge::resolveShortcuts(const std::string& line) const
{
    if (line.empty())
        return line;

    if (line[0] == '?') {
        const std::string rest = trim(line.substr(1));
        return rest.empty() ? "HELP" : "HELP " + rest;
    }

    return line;
}

ShellRunResult ShellBridge::runLine(xbase::DbArea& area, const std::string& line)
{
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
        return { true, true, "" };
    }

    const std::string resolved = resolveShortcuts(trimmed);

    std::istringstream tok(resolved);
    std::string commandToken;
    tok >> commandToken;

    const std::string commandUpper = upcopy(commandToken);

    if (std::getenv("FOXTALK_DEBUG")) {
        const std::streampos pos = tok.tellg();
        if (pos != std::streampos(-1)) {
            const std::string args = resolved.substr(static_cast<std::size_t>(pos));
            std::cout << "[DEBUG args] {" << args << "}\n";
        }
    }

    if (commandUpper == "/SELFTEST" || commandUpper == "SELFTEST" || commandUpper == "HELP") {
        return runBuiltIn(area, resolved, commandUpper, tok);
    }

    return runEngineCommand(area, commandUpper, tok, commandToken, resolved);
}

ShellRunResult ShellBridge::runBuiltIn(xbase::DbArea& area,
                                       const std::string& resolvedLine,
                                       const std::string& commandUpper,
                                       std::istringstream& tok)
{
    (void)area;
    (void)resolvedLine;

    if (commandUpper == "/SELFTEST" || commandUpper == "SELFTEST") {
        std::size_t n = 500;
        if (tok.good())
            tok >> n;

        doSelfTest(n);
        return { true, true, "Selftest complete." };
    }

    if (commandUpper == "HELP") {
        handleHelp(tok);
        return { true, true, "HELP shown." };
    }

    return { false, false, "" };
}

ShellRunResult ShellBridge::runEngineCommand(xbase::DbArea& area,
                                             const std::string& commandUpper,
                                             std::istringstream& tok,
                                             const std::string& originalCommandToken,
                                             const std::string& resolvedLine)
{
    if (!dli::registry().run(area, commandUpper, tok)) {
        const std::string msg = "Unknown or failed command: " + originalCommandToken;
        std::cerr << msg << "\n";
        return { true, false, msg };
    }

    return { true, true, "OK: " + resolvedLine };
}

void ShellBridge::handleHelp(std::istringstream& tok)
{
    std::string topic;
    std::getline(tok, topic);
    topic = trim(topic);

    if (topic.empty()) {
        std::cout
            << "Foxtalk help\n"
            << "  Enter runs. F2 Output; PgUp/PgDn/Home/End scroll.\n"
            << "  Windows: Alt-Z/Ctrl-F5 (Command), Alt-O/Ctrl-F6 (Output).\n"
            << "  Shortcuts: '? <cmd>' = HELP <cmd>, F3=RECORDVIEW.\n"
            << "  Window mgmt: WIN SAVE | WIN RESTORE | WIN DEFAULTS\n";
        return;
    }

    const std::string upper = upcopy(topic);

    if (upper == "BROWSE") {
        std::cout << "BROWSE [EDIT] [FOR <expr>] [FIELDS <list>] [ORDER <tag>]\n";
        return;
    }

    if (upper == "RECORDVIEW" || upper == "RECORD_VIEW") {
        std::cout << "RECORDVIEW - single-record CRUD dialog.\n";
        return;
    }

    if (upper == "USE") {
        std::cout << "USE <dbf> [ALIAS <name>] [EXCLUSIVE]\n";
        return;
    }

    if (upper == "WIN") {
        std::cout << "WIN SAVE | WIN RESTORE | WIN DEFAULTS\n";
        return;
    }

    std::cout << "No inline help for '" << topic << "'. Try: FOXREF " << topic << "\n";
}

void ShellBridge::doSelfTest(std::size_t n)
{
    std::cout << "[SELFTEST] Generating " << n << " lines...\n";

    for (std::size_t i = 1; i <= n; ++i) {
        std::cout << "cout line " << i << "\n";
        std::printf("printf line %zu\n", i);

        if ((i % 50) == 0)
            std::cout.flush();
    }

    std::cerr << "[SELFTEST] Done.\n";
}

} // namespace foxtalk