// src/cli/cmd_dotscript.cpp
// DotScript runner: DOTSCRIPT <file> [QUIET] [STOPONERROR] [ECHO]
// Improvements:
// - Smart finder: tries .dts if missing, ./scripts, exe dir, exe dir/scripts, and parent folders.
// - Keeps absolute path support intact.
// - Echo/Quiet/StopOnError flags unchanged.

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <cctype>
#include <vector>
#include <utility>
#include <filesystem>
#include <unordered_set>

#ifdef _WIN32
  #include <windows.h>
#endif

#include "xbase.hpp"
#include "textio.hpp"
#include "shell_api.hpp"   // shell_dispatch_line(DbArea&, const std::string&)

using xbase::DbArea;
namespace fs = std::filesystem;

namespace {

inline bool is_comment_or_blank(const std::string& raw) {
    size_t i = 0;
    while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
    if (i >= raw.size()) return true;                 // blank
    if (raw[i] == '#' || raw[i] == ';') return true; // # ; comments
    if (i + 1 < raw.size() && raw[i] == '/' && raw[i+1] == '/') return true; // //
    return false;
}

struct Flags {
    bool quiet = false;        // suppress echo of each line
    bool stopOnError = false;  // (reserved; QUIT/EXIT already stops)
    bool echo = false;         // force echo (overrides quiet)
};

Flags parse_flags(std::istringstream& iss) {
    Flags f;
    std::string w;
    while (iss >> w) {
        std::string u = textio::up(w);
        if (u == "QUIET")        f.quiet = true;
        else if (u == "STOPONERROR") f.stopOnError = true;
        else if (u == "ECHO")    f.echo = true;
        else {
            // push token back into stream for caller (we advanced one too far)
            std::string rest;
            std::getline(iss, rest);
            std::string merged = w + rest;
            iss.clear();
            iss.str(merged);
            iss.seekg(0);
            break;
        }
    }
    return f;
}

#ifdef _WIN32
static std::string utf8_from_w(const std::wstring& ws) {
    if (ws.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), size, nullptr, nullptr);
    return out;
}
#endif

static fs::path get_exe_dir() {
#ifdef _WIN32
    std::wstring buf;
    buf.resize(1024);
    DWORD n = GetModuleFileNameW(nullptr, &buf[0], (DWORD)buf.size());
    if (n >= buf.size()) {
        buf.resize(32768);
        n = GetModuleFileNameW(nullptr, &buf[0], (DWORD)buf.size());
    }
    buf.resize(n);
    fs::path p = fs::path(buf).parent_path();
    return p;
#else
    try {
        fs::path p = fs::read_symlink("/proc/self/exe").parent_path();
        return p;
    } catch (...) {
        return fs::current_path();
    }
#endif
}

static bool has_extension(const std::string& name) {
    fs::path p(name);
    return p.has_extension();
}

static bool exists_file(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

// Build a list of candidate paths in order of preference.
static std::vector<fs::path> build_candidates(const std::string& rawName) {
    std::vector<fs::path> out;
    fs::path name = rawName;
    bool addDtsVariant = !has_extension(rawName); // if user didn't specify an extension, try .dts too

    auto add_in = [&](const fs::path& base) {
        out.push_back(base / name);
        if (addDtsVariant) out.push_back(base / (name.string() + ".dts"));
    };

    // 1) As given (absolute or relative)
    out.push_back(name);
    if (addDtsVariant) out.push_back(name.string() + ".dts");

    // 2) Current working directory and ./scripts
    fs::path cwd = fs::current_path();
    add_in(cwd);
    add_in(cwd / "scripts");

    // 3) Executable directory and exeDir/scripts
    fs::path exeDir = get_exe_dir();
    add_in(exeDir);
    add_in(exeDir / "scripts");

    // 4) Walk up parent folders (up to 5 levels) from CWD, try parent and parent/scripts
    fs::path cur = cwd;
    for (int i = 0; i < 5; ++i) {
        cur = cur.parent_path();
        if (cur.empty() || cur == cur.parent_path()) break;
        add_in(cur);
        add_in(cur / "scripts");
    }

    // De-duplicate while preserving order (don't require files to exist).
    std::unordered_set<std::string> seen;
    std::vector<fs::path> uniq;
    uniq.reserve(out.size());
    for (const auto& p : out) {
        // Normalize lexically (no FS access).
        std::string k = p.lexically_normal().generic_string();
        if (seen.insert(k).second) uniq.push_back(p);
    }
    return uniq;
}

static std::pair<bool, fs::path> find_script_file(const std::string& userArg, std::vector<fs::path>* tried = nullptr) {
    auto candidates = build_candidates(userArg);
    for (const auto& p : candidates) {
        if (tried) tried->push_back(p);
        if (exists_file(p)) {
            std::error_code ec;
            // Use weakly_canonical when possible; otherwise fall back to absolute.
            fs::path can = fs::weakly_canonical(p, ec);
            if (ec) can = fs::absolute(p, ec);
            return {true, can};
        }
    }
    return {false, {}};
}

} // anon

// DOTSCRIPT <file> [QUIET] [STOPONERROR] [ECHO]
void cmd_DOTSCRIPT(DbArea& A, std::istringstream& iss) {
    // Parse entire tail first (we allow flags before/after file)
    std::string tail; std::getline(iss, tail);
    tail = textio::trim(tail);
    if (tail.empty()) {
        std::cout << "Usage: DOTSCRIPT <file> [QUIET] [STOPONERROR] [ECHO]\n";
        return;
    }

    auto run_with = [&](const std::string& filename, Flags f) {
        if (f.echo) f.quiet = false;

        std::vector<fs::path> tried;
        auto [ok, path] = find_script_file(filename, &tried);
        if (!ok) {
            std::cout << "DOTSCRIPT: cannot find '" << filename << "'.\n";
            std::cout << "  Tried:\n";
            for (const auto& p : tried) std::cout << "    " << p.string() << "\n";
            return;
        }

        std::ifstream in(path);
        if (!in) { std::cout << "DOTSCRIPT: cannot open '" << path.string() << "'.\n"; return; }

        std::size_t lineNo = 0, ran = 0, skipped = 0, failed = 0;
        std::string line;
        while (std::getline(in, line)) {
            ++lineNo;
            if (is_comment_or_blank(line)) { ++skipped; continue; }
            std::string trimmed = textio::trim(line);
            if (f.echo || (!f.quiet)) {
                std::cout << "[dts:" << lineNo << "] " << trimmed << "\n";
            }
            bool okline = shell_dispatch_line(A, trimmed);
            if (!okline) { std::cout << "DOTSCRIPT: terminated by QUIT/EXIT at line " << lineNo << ".\n"; break; }
            ++ran;
        }
        std::cout << "DOTSCRIPT: file='" << path.string() << "' ran=" << ran
                  << " skipped=" << skipped << " failed=" << failed << "\n";
    };

    // Pass 1: flags then filename
    {
        std::istringstream pass1(tail);
        Flags f = parse_flags(pass1);
        std::string tok;
        if (pass1 >> std::ws && pass1.peek() == '"') {
            pass1.get();
            std::getline(pass1, tok, '"');
        } else {
            pass1 >> tok;
        }
        if (!tok.empty()) {
            std::string after; std::getline(pass1, after);
            std::istringstream pass1b(after);
            Flags f2 = parse_flags(pass1b);
            f.quiet       = (f.quiet       || f2.quiet);
            f.stopOnError = (f.stopOnError || f2.stopOnError);
            f.echo        = (f.echo        || f2.echo);
            run_with(tok, f);
            return;
        }
    }

    // Pass 2: filename then flags
    {
        std::istringstream pass2(tail);
        std::string tok;
        if (pass2 >> std::ws && pass2.peek() == '"') {
            pass2.get();
            std::getline(pass2, tok, '"');
        } else {
            pass2 >> tok;
        }
        if (tok.empty()) {
            std::cout << "Usage: DOTSCRIPT <file> [QUIET] [STOPONERROR] [ECHO]\n";
            return;
        }
        std::string after; std::getline(pass2, after);
        std::istringstream flagss(after);
        Flags f = parse_flags(flagss);
        run_with(tok, f);
    }
}

// Self-register with the command registry
#include "command_registry.hpp"
static bool s_registered = [](){
    cli::registry().add("DOTSCRIPT", &cmd_DOTSCRIPT);
    cli::registry().add("RUN",       &cmd_DOTSCRIPT);
    cli::registry().add("DO",        &cmd_DOTSCRIPT);
    return true;
}();
