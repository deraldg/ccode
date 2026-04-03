#include "cli/zip_service.hpp"

#include <filesystem>
#include <string>

#if !defined(_WIN32)
#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace dottalk::zip::detail {

#if defined(_WIN32)

// Windows build: compile-safe stubs only.
// Real POSIX implementation is only used on non-Windows platforms.

bool posix_available_list(std::string* why_not)
{
    if (why_not) *why_not = "POSIX backend not available on Windows";
    return false;
}

bool posix_available_create(std::string* why_not)
{
    if (why_not) *why_not = "POSIX backend not available on Windows";
    return false;
}

bool posix_available_extract(std::string* why_not)
{
    if (why_not) *why_not = "POSIX backend not available on Windows";
    return false;
}

Result posix_list_archive(const fs::path& archive_path, Listing* listing)
{
    if (listing) listing->entries.clear();

    Result r;
    r.success = false;
    r.exit_code = -1;
    r.backend = Backend::PosixZip;
    r.operation = Operation::List;
    r.archive_path = archive_path;
    r.message = "POSIX backend not available on Windows";
    return r;
}

Result posix_create_archive(const fs::path& archive_path, const fs::path& source_path)
{
    Result r;
    r.success = false;
    r.exit_code = -1;
    r.backend = Backend::PosixZip;
    r.operation = Operation::Create;
    r.archive_path = archive_path;
    r.source_path = source_path;
    r.message = "POSIX backend not available on Windows";
    return r;
}

Result posix_extract_archive(const fs::path& archive_path, const fs::path& target_path)
{
    Result r;
    r.success = false;
    r.exit_code = -1;
    r.backend = Backend::PosixZip;
    r.operation = Operation::Extract;
    r.archive_path = archive_path;
    r.target_path = target_path;
    r.message = "POSIX backend not available on Windows";
    return r;
}

#else

namespace {

static std::string shell_quote(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'')
            out += "'\"'\"'";
        else
            out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

struct ExecResult {
    int exit_code = -1;
    std::string output;
};

static ExecResult run_capture(const std::string& cmd)
{
    ExecResult r;

    std::array<char, 4096> buf{};
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
        r.exit_code = -1;
        r.output = "unable to start process";
        return r;
    }

    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        r.output += buf.data();
    }

    int raw = pclose(pipe);
    if (WIFEXITED(raw))
        r.exit_code = WEXITSTATUS(raw);
    else
        r.exit_code = raw;

    return r;
}

static bool command_exists(const char* name)
{
    const std::string cmd = std::string("command -v ") + name + " >/dev/null";
    ExecResult r = run_capture(cmd);
    return r.exit_code == 0;
}

static std::vector<std::string> split_lines_nonempty(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream iss(text);
    std::string line;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
            out.push_back(line);
    }
    return out;
}

} // namespace

bool posix_available_list(std::string* why_not)
{
    if (!command_exists("unzip")) {
        if (why_not) *why_not = "required tool 'unzip' not found";
        return false;
    }
    return true;
}

bool posix_available_create(std::string* why_not)
{
    if (!command_exists("zip")) {
        if (why_not) *why_not = "required tool 'zip' not found";
        return false;
    }
    return true;
}

bool posix_available_extract(std::string* why_not)
{
    if (!command_exists("unzip")) {
        if (why_not) *why_not = "required tool 'unzip' not found";
        return false;
    }
    return true;
}

Result posix_list_archive(const fs::path& archive_path, Listing* listing)
{
    Result r;
    r.backend = Backend::PosixZip;
    r.operation = Operation::List;
    r.archive_path = archive_path;

    const std::string cmd = "unzip -Z1 " + shell_quote(archive_path.string());
    ExecResult er = run_capture(cmd);

    r.exit_code = er.exit_code;
    r.stdout_text = er.output;

    if (er.exit_code != 0) {
        r.success = false;
        r.message = "archive read failed";
        r.stderr_text = er.output;
        return r;
    }

    if (listing)
        listing->entries = split_lines_nonempty(er.output);

    r.success = true;
    r.message = "ok";
    return r;
}

Result posix_create_archive(const fs::path& archive_path, const fs::path& source_path)
{
    Result r;
    r.backend = Backend::PosixZip;
    r.operation = Operation::Create;
    r.archive_path = archive_path;
    r.source_path = source_path;

    const fs::path parent = source_path.parent_path().empty() ? fs::current_path() : source_path.parent_path();
    const fs::path leaf   = source_path.filename();

    const std::string cmd =
        "cd " + shell_quote(parent.string()) +
        " && zip -r -q " + shell_quote(archive_path.string()) +
        " " + shell_quote(leaf.string());

    ExecResult er = run_capture(cmd);

    r.exit_code = er.exit_code;
    r.stdout_text = er.output;

    if (er.exit_code != 0) {
        r.success = false;
        r.message = "archive creation failed";
        r.stderr_text = er.output;
        return r;
    }

    r.success = true;
    r.message = "ok";
    return r;
}

Result posix_extract_archive(const fs::path& archive_path, const fs::path& target_path)
{
    Result r;
    r.backend = Backend::PosixZip;
    r.operation = Operation::Extract;
    r.archive_path = archive_path;
    r.target_path = target_path;

    const std::string cmd =
        "unzip -q " + shell_quote(archive_path.string()) +
        " -d " + shell_quote(target_path.string());

    ExecResult er = run_capture(cmd);

    r.exit_code = er.exit_code;
    r.stdout_text = er.output;

    if (er.exit_code != 0) {
        r.success = false;
        r.message = "extraction failed";
        r.stderr_text = er.output;
        return r;
    }

    r.success = true;
    r.message = "ok";
    return r;
}

#endif

} // namespace dottalk::zip::detail