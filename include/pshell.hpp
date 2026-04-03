// pshell.hpp
#pragma once
#include <string>
#include <vector>
#include <string_view>

namespace pshell {

struct Item {
    const char* name;      // canonical upper-case task name
    const char* syntax;    // PowerShell command syntax
    const char* summary;   // short description
    bool supported;        // always true for reference entries
};

inline const std::vector<Item>& catalog() {
    static const std::vector<Item> k = {

        // ---------------------------------------------------------------------
        // SEARCH / DISCOVERY
        // ---------------------------------------------------------------------
        {"SEARCH-FILES", 
         "Get-ChildItem -Recurse -Filter <pattern>",
         "Find files recursively by name or pattern.", true},

        {"SEARCH-STRING", 
         "Select-String -Pattern <text> -Recurse",
         "Search for text across all files in the repo.", true},

        {"SEARCH-SYMBOL-CPP",
         "Select-String -Pattern \"class |struct |namespace \" -Recurse -Filter *.cpp,*.hpp",
         "Locate C++ structural symbols across the codebase.", true},

        {"SEARCH-SYMBOL-PY",
         "Select-String -Pattern \"def |class \" -Recurse -Filter *.py",
         "Locate Python functions and classes.", true},

        {"SEARCH-RECENT",
         "Get-ChildItem -Recurse | Where-Object {$_.LastWriteTime -gt (Get-Date).AddDays(-N)}",
         "Find recently modified files.", true},

        // ---------------------------------------------------------------------
        // REFACTOR / TRANSFORM
        // ---------------------------------------------------------------------
        {"REPLACE-IN-FILES",
         "(Get-Content <file>) -replace \"old\",\"new\" | Set-Content <file>",
         "Replace text inside a file or set of files.", true},

        {"REPLACE-IN-REPO",
         "Get-ChildItem -Recurse -Filter <ext> | ForEach-Object { (Get-Content $_) -replace \"old\",\"new\" | Set-Content $_ }",
         "Perform bulk replacements across the repository.", true},

        {"STRIP-WHITESPACE",
         "Get-ChildItem -Recurse -File | ForEach-Object { (Get-Content $_) -replace \"\\s+$\",\"\" | Set-Content $_ }",
         "Remove trailing whitespace from all files.", true},

        {"NORMALIZE-LINE-ENDINGS",
         "Get-ChildItem -Recurse -File | ForEach-Object { (Get-Content $_) -replace \"`r`n\",\"`n\" | Set-Content $_ }",
         "Convert CRLF to LF across the repo.", true},

        {"TABS-TO-SPACES",
         "Get-ChildItem -Recurse -Filter *.cpp,*.hpp | ForEach-Object { (Get-Content $_) -replace \"`t\",\"    \" | Set-Content $_ }",
         "Convert tabs to spaces in C++ source files.", true},

        // ---------------------------------------------------------------------
        // FILE / DIRECTORY OPERATIONS
        // ---------------------------------------------------------------------
        {"COPY-TREE",
         "Copy-Item -Recurse <src> <dest>",
         "Copy a directory tree.", true},

        {"MOVE-FILE",
         "Move-Item <src> <dest>",
         "Move or rename a file.", true},

        {"DELETE-TREE",
         "Remove-Item -Recurse -Force <path>",
         "Delete a directory tree.", true},

        {"CLEAN-TEMP",
         "Get-ChildItem -Recurse -Filter *.tmp | Remove-Item -Force",
         "Remove temporary files.", true},

        {"CLEAN-BUILD",
         "Get-ChildItem -Recurse -Filter bin,obj | Remove-Item -Recurse -Force",
         "Remove build artifacts.", true},

        // ---------------------------------------------------------------------
        // FILE INSPECTION
        // ---------------------------------------------------------------------
        {"OPEN-NOTEPAD",
         "notepad <file>",
         "Open a file in Notepad.", true},

        {"OPEN-VSCODE",
         "code .",
         "Open the current folder in VS Code.", true},

        {"TAIL-FILE",
         "Get-Content <file> -Tail 50",
         "View the last 50 lines of a file.", true},

        {"WATCH-FILE",
         "Get-Content <file> -Wait",
         "Stream file updates (log tail).", true},

        {"COUNT-LINES",
         "Get-Content <file> | Measure-Object -Line",
         "Count lines in a file.", true},

        // ---------------------------------------------------------------------
        // GIT / VERSION CONTROL
        // ---------------------------------------------------------------------
        {"GIT-STATUS",
         "git status",
         "Show working tree status.", true},

        {"GIT-DIFF",
         "git diff",
         "Show unstaged changes.", true},

        {"GIT-GREP",
         "git grep <pattern>",
         "Search tracked files for text.", true},

        {"GIT-CLEAN",
         "git clean -fd",
         "Remove untracked files and directories.", true},

        {"GIT-BLAME",
         "git blame <file>",
         "Show line-by-line authorship.", true},

        // ---------------------------------------------------------------------
        // PYTHON WORKFLOW
        // ---------------------------------------------------------------------
        {"PY-VENV-CREATE",
         "python -m venv .venv",
         "Create a Python virtual environment.", true},

        {"PY-VENV-ACTIVATE",
         ".\\.venv\\Scripts\\activate",
         "Activate the virtual environment.", true},

        {"PY-INSTALL-REQS",
         "pip install -r requirements.txt",
         "Install dependencies from requirements.txt.", true},

        {"PY-FREEZE",
         "pip freeze > requirements.txt",
         "Export installed packages.", true},

        {"PY-TEST",
         "pytest -q",
         "Run Python tests quietly.", true},

        {"PY-PROFILE",
         "python -m cProfile script.py",
         "Profile a Python script.", true},

        // ---------------------------------------------------------------------
        // C++ WORKFLOW
        // ---------------------------------------------------------------------
        {"CPP-BUILD-GCC",
         "g++ -std=c++20 -O2 -Wall -Wextra -o app.exe *.cpp",
         "Compile all C++ files with GCC.", true},

        {"CPP-BUILD-CLANG-ASAN",
         "clang++ -std=c++20 -fsanitize=address -g *.cpp -o app_asan.exe",
         "Build with Clang and AddressSanitizer.", true},

        {"CPP-CMAKE-CONFIG",
         "cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug",
         "Configure a CMake project.", true},

        {"CPP-CMAKE-BUILD",
         "cmake --build build --config Release",
         "Build a CMake project.", true},

        {"CPP-STATIC-ANALYSIS",
         "clang-tidy *.cpp -- -std=c++20",
         "Run Clang-Tidy static analysis.", true},

        // ---------------------------------------------------------------------
        // SYSTEM / ENVIRONMENT
        // ---------------------------------------------------------------------
        {"PROC-LIST",
         "Get-Process | Sort-Object CPU -Descending",
         "List processes sorted by CPU usage.", true},

        {"PROC-KILL",
         "Stop-Process -Name <name>",
         "Terminate a process by name.", true},

        {"ENV-LIST",
         "Get-ChildItem Env:",
         "List environment variables.", true},

        {"FIND-EXECUTABLE",
         "where <program>",
         "Locate an executable in PATH.", true},

        {"PORTS-LISTENING",
         "Get-NetTCPConnection | Where-Object {$_.State -eq \"Listen\"}",
         "List listening TCP ports.", true},

        // ---------------------------------------------------------------------
        // ARCHIVING / DIFF / PATCH
        // ---------------------------------------------------------------------
        {"TAR-CREATE",
         "tar -cvf <archive.tar> <folder>",
         "Create a tar archive.", true},

        {"TAR-EXTRACT",
         "tar -xvf <archive.tar>",
         "Extract a tar archive.", true},

        {"DIFF-FILES",
         "diff (Get-Content f1) (Get-Content f2)",
         "Compare two text files.", true},

        {"GIT-APPLY-PATCH",
         "git apply <patch.diff>",
         "Apply a patch file.", true},

    };
    return k;
}

} // namespace pshell