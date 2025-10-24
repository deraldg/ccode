// src/cli/cmd_test.cpp
#include "xbase.hpp"
#include "textio.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <chrono>

#include "shell_api.hpp"

// NOTE: tiny hook exposed by shell.cpp (see snippet below)
bool shell_dispatch_line(xbase::DbArea& A, const std::string& line);

using namespace xbase;

namespace {
    inline bool is_comment_or_blank(const std::string& s0) {
        std::string s = textio::trim(s0);
        if (s.empty()) return true;
        if (s.rfind("#", 0) == 0) return true;
        if (s.rfind("//", 0) == 0) return true;
        return false;
    }
}

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

    std::ifstream fin(scriptPath);
    if (!fin) {
        std::cout << "TEST: cannot open script: " << scriptPath << "\n";
        return;
    }

    std::ofstream flog;
    const bool wantLog = !logPath.empty() && logPath != "-";
    if (wantLog) {
        flog.open(logPath, std::ios::out | std::ios::trunc);
        if (!flog) {
            std::cout << "TEST: cannot open log: " << logPath << "\n";
            return;
        }
    }

    auto t0 = std::chrono::steady_clock::now();
    std::string line;
    size_t nlines = 0, nrun = 0, nerr = 0;

    while (std::getline(fin, line)) {
        ++nlines;
        if (is_comment_or_blank(line)) continue;

        if (verbose) {
            std::cout << "> " << line << "\n";
            if (wantLog) flog << "> " << line << "\n";
        }

        // Dispatch through the regular shell machinery
        bool ok = shell_dispatch_line(A, line);
        if (!ok) {
            ++nerr;
            std::cout << "TEST: command failed on line " << nlines << ": " << line << "\n";
            if (wantLog) flog << "TEST: command failed on line " << nlines << ": " << line << "\n";
            // Keep going (don’t fail-fast by default)
        }
        ++nrun;
    }

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "TEST: " << nrun << " lines processed, " << nerr
              << " error(s), duration " << sec << "s\n";
    if (wantLog) {
        flog << "TEST: " << nrun << " lines processed, " << nerr
             << " error(s), duration " << sec << "s\n";
    }
}
