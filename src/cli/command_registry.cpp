// ============================================================================
// File: src/cli/command_registry.cpp
// Project: DotTalk++
// ----------------------------------------------------------------------------
#include "command_registry.hpp"

#include "textio.hpp"

#include <cctype>
#include <iostream>
#include <vector>

using xbase::DbArea;

namespace dli {

static thread_local std::vector<std::string> tl_argv;
static thread_local std::string tl_raw_args;

const std::vector<std::string>& current_argv() noexcept { return tl_argv; }
const std::string& current_raw_args() noexcept { return tl_raw_args; }

static inline void ltrim_stream(std::istringstream& iss) {
    while (iss && std::isspace(static_cast<unsigned char>(iss.peek())))
        iss.get();
}

void CommandRegistry::add(const std::string& name, Handler h) {
    map_[name] = std::move(h);
}

RunResult CommandRegistry::try_run(DbArea& area,
                                   const std::string& normalized_key,
                                   std::istringstream& args)
{
    auto it = map_.find(normalized_key);
    if (it == map_.end())
        return {RunStatus::UnknownCommand, false, "Unknown command: " + normalized_key};

    // Snapshot remaining args without consuming the stream (handlers may still parse args).
    std::string raw;
    if (auto pos = args.tellg(); pos != std::istringstream::pos_type(-1)) {
        const std::string& whole = args.str();
        const auto upos = static_cast<size_t>(pos);
        if (upos < whole.size())
            raw = whole.substr(upos);
    }

    struct Scope {
        explicit Scope(std::string raw_in) {
            tl_raw_args = std::move(raw_in);
            tl_argv = textio::tokenize(tl_raw_args);
        }
        ~Scope() noexcept {
            tl_argv.clear();
            tl_raw_args.clear();
        }
    } scope{raw};

    try {
        ltrim_stream(args);
        it->second(area, args);
        return {RunStatus::Ok, false, {}};
    } catch (const ExitRequested&) {
        return {RunStatus::Ok, true, {}};
    } catch (const std::exception& e) {
        return {RunStatus::HandlerError, false, std::string("Command error: ") + e.what()};
    } catch (...) {
        return {RunStatus::HandlerError, false, "Command error: unknown exception"};
    }
}

bool CommandRegistry::run(DbArea& area,
                          const std::string& normalized_key,
                          std::istringstream& args)
{
    const RunResult r = try_run(area, normalized_key, args);

    if (r.exit_requested)
        return false; // stop shell

    switch (r.status) {
        case RunStatus::Ok:
            return true;
        case RunStatus::UnknownCommand:
            std::cout << r.message << "\n";
            return true;
        case RunStatus::HandlerError:
            std::cerr << r.message << "\n";
            return true;
    }
    return true;
}

// Singleton instance
static CommandRegistry& instance() {
    static CommandRegistry reg;
    return reg;
}

CommandRegistry& registry() { return instance(); }

void register_command(const std::string& name, dli::Handler h) {
    registry().add(name, std::move(h));
}

const std::unordered_map<std::string, dli::Handler>& map() {
    return registry().map();
}

} // namespace dli