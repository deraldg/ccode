// src/cli/cmd_test.cpp
#include "xbase.hpp"
#include "textio.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <chrono>

#include <filesystem>

#include "shell_api.hpp"  // must declare: bool shell_dispatch_line(xbase::DbArea&, const std::string&);

#if __has_include("cli/path_resolver.hpp") && __has_include("cli/cmd_setpath.hpp")
  #include "cli/path_resolver.hpp"
  #include "cli/cmd_setpath.hpp"
  #define HAVE_PATHS 1
#else
  #define HAVE_PATHS 0
#endif


using namespace xbase;

namespace {

// trim helpers
inline std::string ltrim(std::string s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
inline std::string rtrim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}
inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

// scan the line, tracking quotes and backslash-escapes
struct ScanState {
    bool in_single = false;
    bool in_double = false;
};

static void step_quote(ScanState &st, char c, char prev) {
    if (c == '\'' && !st.in_double && prev != '\\') st.in_single = !st.in_single;
    else if (c == '\"' && !st.in_single && prev != '\\') st.in_double = !st.in_double;
}

// Remove trailing comment that starts with '#' or '//' when not inside quotes.
static std::string strip_unquoted_comment(std::string s) {
    ScanState st{};
    char prev = '\0';
    // First, handle '//' which has 2 chars
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        step_quote(st, s[i], prev);
        if (!st.in_single && !st.in_double) {
            if (s[i] == '/' && s[i+1] == '/') {
                s.resize(i);
                return rtrim(std::move(s));
            }
        }
        prev = s[i];
    }
    // Then handle single '#'
    st = {};
    prev = '\0';
    for (size_t i = 0; i < s.size(); ++i) {
        step_quote(st, s[i], prev);
        if (!st.in_single && !st.in_double && s[i] == '#') {
            s.resize(i);
            break;
        }
        prev = s[i];
    }
    return rtrim(std::move(s));
}

// Does the *last non-space* character equal ';' and is it OUTSIDE quotes?
static bool ends_with_unquoted_semicolon(const std::string &line) {
    // find last non-space
    if (line.empty()) return false;
    size_t j = line.size();
    while (j > 0 && std::isspace(static_cast<unsigned char>(line[j-1]))) --j;
    if (j == 0) return false;
    if (line[j-1] != ';') return false;

    // check quoting up to that position
    ScanState st{};
    char prev = '\0';
    for (size_t i = 0; i < j - 1; ++i) {
        step_quote(st, line[i], prev);
        prev = line[i];
    }
    // if not inside quotes at the semicolon -> continuation
    return !(st.in_single || st.in_double);
}

// Remove the *trailing* unquoted semicolon (and trailing spaces before it)
static void chop_trailing_unquoted_semicolon(std::string &line) {
    // find last non-space
    if (line.empty()) return;
    size_t j = line.size();
    while (j > 0 && std::isspace(static_cast<unsigned char>(line[j-1]))) --j;
    if (j == 0 || line[j-1] != ';') return;

    // verify unquoted
    ScanState st{};
    char prev = '\0';
    for (size_t i = 0; i < j - 1; ++i) {
        step_quote(st, line[i], prev);
        prev = line[i];
    }
    if (st.in_single || st.in_double) return;

    // chop: remove up to and including the ';', then trim trailing spaces again
    line.resize(j - 1);
    while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
}

// comment/blank check after trimming+comment-strip
inline bool is_comment_or_blank(const std::string& s0) {
    std::string s = trim(strip_unquoted_comment(s0));
    return s.empty();
}

} // namespace

// TEST <scriptfile> [<logfile>] [VERBOSE]
void cmd_TEST(DbArea& A, std::istringstream& in)
{
    std::string scriptPath;
    std::string logPath;
    std::string maybeVerbose;

    if (!(in >> scriptPath)) {
        std::cout << "Usage: TEST <scriptfile> [<logfile>] [VERBOSE]\n";
        return;
    }
    // logfile is optional
    if (in >> logPath) {
        // third token might be VERBOSE, allow: TEST file VERBOSE
        if (textio::ieq(logPath, "VERBOSE")) {
            maybeVerbose = logPath;
            logPath.clear();
        } else {
            in >> maybeVerbose; // might be VERBOSE or nothing
        }
    }
    const bool verbose = textio::ieq(maybeVerbose, "VERBOSE");

    namespace fs = std::filesystem;

    fs::path scriptResolved = shell_resolve_script_path(scriptPath);

    std::string pushErr;
    if (!shell_script_push(scriptResolved, /*as_subscript=*/false, &pushErr)) {
        std::cout << "TEST: " << pushErr << ": " << scriptResolved.string() << "\n";
        return;
    }

    std::ifstream fin(scriptResolved, std::ios::binary);
    if (!fin) {
        shell_script_pop();
        std::cout << "TEST: cannot open script: " << scriptResolved.string() << "\n";
        return;
    }

    std::ofstream flog;
    const bool wantLog = !logPath.empty() && logPath != "-";
    if (wantLog) {
        flog.open(logPath, std::ios::out | std::ios::trunc);
        if (!flog) {
            std::cout << "TEST: cannot open log: " << logPath << "\n";
            shell_script_pop();
            return;
        }
    }

    auto t0 = std::chrono::steady_clock::now();
    std::string line;
    std::string buffer;        // accumulates continued lines
    size_t nlines = 0, nrun = 0, nerr = 0;

    while (std::getline(fin, line)) {
        ++nlines;

        // Strip inline comments (unquoted), then trim
        std::string work = trim(strip_unquoted_comment(line));
        if (work.empty()) continue;

        // Continuation?
        const bool cont = ends_with_unquoted_semicolon(work);
        if (cont) {
            chop_trailing_unquoted_semicolon(work);
            if (!buffer.empty()) buffer += ' ';
            buffer += work;
            continue; // read next physical line
        }

        // Final line of the logical command
        if (!buffer.empty()) buffer += ' ';
        buffer += work;

        if (verbose) {
            std::cout << "> " << buffer << "\n";
            if (wantLog) flog << "> " << buffer << "\n";
        }

        bool ok = shell_execute_line(A, buffer);
        if (!ok) {
            ++nerr;
            std::cout << "TEST: command failed on/near line " << nlines << ": " << buffer << "\n";
            if (wantLog) flog << "TEST: command failed on/near line " << nlines << ": " << buffer << "\n";
        }
        ++nrun;

        buffer.clear(); // reset for next logical command
    }

    // Handle file ending with a dangling continuation (treat as a complete command)
    if (!buffer.empty()) {
        if (verbose) {
            std::cout << "> " << buffer << "\n";
            if (wantLog) flog << "> " << buffer << "\n";
        }
        bool ok = shell_execute_line(A, buffer);
        if (!ok) {
            ++nerr;
            std::cout << "TEST: command failed at EOF: " << buffer << "\n";
            if (wantLog) flog << "TEST: command failed at EOF: " << buffer << "\n";
        }
        ++nrun;
    }

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "TEST: " << nrun << " line(s) processed, " << nerr
              << " error(s), duration " << sec << "s\n";
    if (wantLog) {
        flog << "TEST: " << nrun << " line(s) processed, " << nerr
             << " error(s), duration " << sec << "s\n";
    }

    shell_script_pop();
}
