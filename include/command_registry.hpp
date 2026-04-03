// ============================================================================
// File: src/cli/command_registry.hpp
// Project: DotTalk++
// ----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace xbase { class DbArea; }

namespace dli {

using Handler = std::function<void(xbase::DbArea& area, std::istringstream& args)>;

/**
 * Handlers may throw this to request a clean shutdown (EXIT/QUIT).
 * CommandRegistry::try_run catches it and reports exit_requested=true.
 */
struct ExitRequested final : std::exception {
    const char* what() const noexcept override { return "exit requested"; }
};

enum class RunStatus : std::uint8_t {
    Ok = 0,
    UnknownCommand,
    HandlerError,
};

struct RunResult {
    RunStatus status{RunStatus::Ok};
    bool exit_requested{false};
    std::string message; // for UnknownCommand / HandlerError
};

class CommandRegistry final {
public:
    void add(const std::string& name, Handler h);

    /**
     * Legacy contract: returns "keep shell alive".
     * Prints errors to stdout/stderr. Returns false if exit_requested.
     */
    bool run(xbase::DbArea& area,
             const std::string& normalized_key,
             std::istringstream& args);

    /**
     * UI-friendly contract: does not print.
     * - status==Ok: handler ran successfully
     * - status==UnknownCommand: no handler registered
     * - status==HandlerError: handler threw
     * - exit_requested==true: handler requested shutdown (status stays Ok)
     */
    RunResult try_run(xbase::DbArea& area,
                      const std::string& normalized_key,
                      std::istringstream& args);

    const std::unordered_map<std::string, Handler>& map() const noexcept { return map_; }

private:
    std::unordered_map<std::string, Handler> map_;
};

CommandRegistry& registry();
void register_command(const std::string& name, Handler h);
const std::unordered_map<std::string, Handler>& map();

/**
 * Quote-aware argv tokens for the currently executing command.
 * Valid only while a handler is running (inside try_run/run).
 */
const std::vector<std::string>& current_argv() noexcept;

/**
 * Raw args substring for the currently executing command (the remainder of args stream).
 * Valid only while a handler is running (inside try_run/run).
 */
const std::string& current_raw_args() noexcept;

} // namespace dli