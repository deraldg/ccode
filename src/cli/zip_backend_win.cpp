#include "cli/zip_service.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace dottalk::zip::detail {

namespace {

static std::string ps_single_quote(const std::string& s)
{
    // PowerShell single-quoted string escaping:
    // abc'def -> 'abc''def'
    std::string out;
    out.reserve(s.size() + 8);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'')
            out += "''";
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

#if defined(_WIN32)
    FILE* pipe = _popen((cmd + " 2>&1").c_str(), "r");
#else
    FILE* pipe = ::popen((cmd + " 2>&1").c_str(), "r");
#endif
    if (!pipe) {
        r.exit_code = -1;
        r.output = "unable to start process";
        return r;
    }

    std::array<char, 4096> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        r.output += buf.data();
    }

#if defined(_WIN32)
    r.exit_code = _pclose(pipe);
#else
    r.exit_code = ::pclose(pipe);
#endif
    return r;
}

static bool powershell_exists()
{
#if defined(_WIN32)
    ExecResult r = run_capture("where powershell");
    if (r.exit_code == 0)
        return true;

    r = run_capture("where pwsh");
    return r.exit_code == 0;
#else
    return false;
#endif
}

static std::string powershell_exe()
{
#if defined(_WIN32)
    ExecResult r = run_capture("where powershell");
    if (r.exit_code == 0)
        return "powershell";
    return "pwsh";
#else
    return "powershell";
#endif
}

static std::string build_ps_command(const std::string& ps_script)
{
    return powershell_exe() + " -NoProfile -ExecutionPolicy Bypass -Command " + ps_single_quote(ps_script);
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

bool win_available_list(std::string* why_not)
{
#if defined(_WIN32)
    if (!powershell_exists()) {
        if (why_not) *why_not = "PowerShell not found";
        return false;
    }
    return true;
#else
    if (why_not) *why_not = "Windows backend not available on this platform";
    return false;
#endif
}

bool win_available_create(std::string* why_not)
{
#if defined(_WIN32)
    if (!powershell_exists()) {
        if (why_not) *why_not = "PowerShell not found";
        return false;
    }
    return true;
#else
    if (why_not) *why_not = "Windows backend not available on this platform";
    return false;
#endif
}

bool win_available_extract(std::string* why_not)
{
#if defined(_WIN32)
    if (!powershell_exists()) {
        if (why_not) *why_not = "PowerShell not found";
        return false;
    }
    return true;
#else
    if (why_not) *why_not = "Windows backend not available on this platform";
    return false;
#endif
}

Result win_list_archive(const fs::path& archive_path, Listing* listing)
{
    Result r;
    r.backend = Backend::PowerShellArchive;
    r.operation = Operation::List;
    r.archive_path = archive_path;

#if !defined(_WIN32)
    r.success = false;
    r.exit_code = -1;
    r.message = "Windows backend not available on this platform";
    return r;
#else
    const std::string ap = archive_path.string();

    std::ostringstream ps;
    ps
        << "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        << "$zip=[System.IO.Compression.ZipFile]::OpenRead(" << ps_single_quote(ap) << "); "
        << "try { $zip.Entries | ForEach-Object { $_.FullName } } finally { $zip.Dispose() }";

    ExecResult er = run_capture(build_ps_command(ps.str()));
    r.exit_code = er.exit_code;
    r.stdout_text = er.output;

    if (er.exit_code != 0) {
        r.success = false;
        r.message = "archive read failed";
        r.stderr_text = er.output;
        return r;
    }

    if (listing) {
        listing->entries = split_lines_nonempty(er.output);
    }

    r.success = true;
    r.message = "ok";
    return r;
#endif
}

Result win_create_archive(const fs::path& archive_path, const fs::path& source_path)
{
    Result r;
    r.backend = Backend::PowerShellArchive;
    r.operation = Operation::Create;
    r.archive_path = archive_path;
    r.source_path = source_path;

#if !defined(_WIN32)
    r.success = false;
    r.exit_code = -1;
    r.message = "Windows backend not available on this platform";
    return r;
#else
    const std::string ap = archive_path.string();
    const std::string sp = source_path.string();

    std::ostringstream ps;
    ps
        << "Compress-Archive -LiteralPath "
        << ps_single_quote(sp)
        << " -DestinationPath "
        << ps_single_quote(ap)
        << " -CompressionLevel Optimal";

    ExecResult er = run_capture(build_ps_command(ps.str()));
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
#endif
}

Result win_extract_archive(const fs::path& archive_path, const fs::path& target_path)
{
    Result r;
    r.backend = Backend::PowerShellArchive;
    r.operation = Operation::Extract;
    r.archive_path = archive_path;
    r.target_path = target_path;

#if !defined(_WIN32)
    r.success = false;
    r.exit_code = -1;
    r.message = "Windows backend not available on this platform";
    return r;
#else
    const std::string ap = archive_path.string();
    const std::string tp = target_path.string();

    std::ostringstream ps;
    ps
        << "Expand-Archive -LiteralPath "
        << ps_single_quote(ap)
        << " -DestinationPath "
        << ps_single_quote(tp);

    ExecResult er = run_capture(build_ps_command(ps.str()));
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
#endif
}

} // namespace dottalk::zip::detail