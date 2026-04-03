// pshell_ref.hpp
#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <set>
#include <algorithm>

namespace pshell {

struct Item {
    const char* name;       // short UPPERCASE-HYPHENATED ID
    const char* syntax;     // the actual one-liner or short script
    const char* summary;    // one-line description
    const char* category;   // grouping name
    bool supported = true;
};

inline const std::vector<Item>& catalog() {
    static const std::vector<Item> items = {
        // ────────────────────────────────────────────────
        // File Search / Discovery
        // ────────────────────────────────────────────────
        {"SEARCH-FILES",        "Get-ChildItem -Recurse -Filter *.cpp",                              "Find all .cpp files recursively", "File Search"},
        {"SEARCH-EXT",          "Get-ChildItem -Recurse -Include *.h,*.hpp",                         "Find header files", "File Search"},
        {"SEARCH-INCLUDE",      "Get-ChildItem -Recurse -Include *.cpp,*.hpp,*.cs",                  "Find multiple extensions", "File Search"},
        {"SEARCH-SIZE-GT",      "Get-ChildItem -Recurse -File | Where-Object {$_.Length -gt 1MB}",   "Files larger than 1MB", "File Search"},
        {"SEARCH-RECENT",       "Get-ChildItem -Recurse | Where-Object {$_.LastWriteTime -gt (Get-Date).AddDays(-1)}", "Files modified in last day", "File Search"},
        {"SEARCH-LOG-SORT",     "Get-ChildItem -Recurse -Filter *.log | Sort-Object Length -Descending", "Largest log files", "File Search"},
        {"SEARCH-JSON",         "Get-ChildItem -Recurse -Filter *.json | Select-Object FullName",    "List all JSON files", "File Search"},
        {"SEARCH-DLL",          "Get-ChildItem -Recurse -Filter *.dll",                              "Find all DLLs", "File Search"},
        {"SEARCH-EXE",          "Get-ChildItem -Recurse -Filter *.exe",                              "Find all EXEs", "File Search"},
        {"SEARCH-DIRS",         "Get-ChildItem -Recurse -Directory",                                 "List all directories recursively", "File Search"},
        {"SEARCH-SUM-SIZE",     "Get-ChildItem -Recurse -File | Measure-Object Length -Sum",         "Total size of all files", "File Search"},

        // ────────────────────────────────────────────────
        // Text / String Search in Files
        // ────────────────────────────────────────────────
        {"GREP-TODO",           "Select-String -Pattern \"TODO\" -Recurse",                          "Find TODO comments repo-wide", "Text Search"},
        {"GREP-ERROR",          "Select-String -Pattern \"ERROR\" -Recurse",                         "Find ERROR lines in logs", "Text Search"},
        {"GREP-CLASS",          "Select-String -Pattern \"class \" -Filter *.hpp -Recurse",          "Find C++ class definitions", "Text Search"},
        {"GREP-FAILED",         "Select-String -Pattern \"Failed to open index:\" -Recurse",         "Find index open failures", "Text Search"},
        {"GREP-FIXME",          "Select-String -Pattern \"FIXME\" -Recurse",                         "Find FIXME markers", "Text Search"},
        {"GREP-PASSWORD",       "Select-String -Pattern \"password\" -Recurse",                      "Find potential password strings", "Text Search"},
        {"GREP-UPPER-WORDS",    "Select-String -Pattern \"\\b[A-Z]{2,}\\b\" -Recurse -AllMatches",   "Find acronyms / uppercase words", "Text Search"},
        {"GREP-HTTP",           "Select-String -Pattern \"http://\" -Recurse",                       "Find hardcoded URLs", "Text Search"},
        {"GREP-DEPRECATED",     "Select-String -Pattern \"deprecated\" -Recurse",                    "Find deprecated usage", "Text Search"},
        {"GREP-JOIN-EMIT",      "Get-ChildItem -Recurse -Filter *.cpp | Select-String \"join_emit\"", "Find specific function calls", "Text Search"},

        // ────────────────────────────────────────────────
        // Replace / Refactor in Files
        // ────────────────────────────────────────────────
        {"REPLACE-SINGLE",      "(Get-Content file.txt) -replace \"old\",\"new\" | Set-Content file.txt", "Replace text in one file", "Replace"},
        {"REPLACE-BULK",        "Get-ChildItem -Recurse -Filter *.cpp | ForEach-Object { ($_ | Get-Content) -replace \"foo\",\"bar\" | Set-Content $_ }", "Bulk replace in repo", "Replace"},
        {"REPLACE-TRUE-FALSE",  "Get-ChildItem -Recurse -Filter *.json | ForEach-Object { ($_ | Get-Content) -replace \"true\",\"false\" | Set-Content $_ }", "Toggle booleans in JSON", "Replace"},
        {"REPLACE-TRAILING",    "Get-ChildItem -Recurse -File | ForEach-Object { ($_ | Get-Content) -replace \"\\s+$\",\"\" | Set-Content $_ }", "Remove trailing whitespace", "Replace"},
        {"REPLACE-TABS",        "Get-ChildItem -Recurse -Filter *.cpp,*.hpp | ForEach-Object { ($_ | Get-Content) -replace \"\\t\",\"    \" | Set-Content $_ }", "Tabs → 4 spaces", "Replace"},
        {"REPLACE-COMMENTS",    "Get-ChildItem -Recurse -Filter *.cpp | ForEach-Object { ($_ | Get-Content) -replace \"//.*\",\"\" | Set-Content $_ }", "Strip C++ line comments", "Replace"},
        {"REPLACE-XML-COMMENT", "Get-ChildItem -Recurse -Filter *.xml | ForEach-Object { ($_ | Get-Content) -replace \"<!--.*?-->\",\"\" | Set-Content $_ }", "Remove XML comments", "Replace"},
        {"REPLACE-CRLF",        "Get-ChildItem -Recurse -Filter *.ps1 | ForEach-Object { ($_ | Get-Content) -replace \"\\r\\n\",\"\\n\" | Set-Content $_ }", "Convert CRLF → LF", "Replace"},
        {"REPLACE-SYMBOL",      "Get-ChildItem -Recurse -File *.cpp,*.hpp | ForEach-Object { ($_ | Get-Content) -replace \"OldName\",\"NewName\" | Set-Content $_ }", "Repo-wide symbol rename", "Replace"},

        // ────────────────────────────────────────────────
        // File & Directory Operations
        // ────────────────────────────────────────────────
        {"COPY-RECURSE",        "Copy-Item -Recurse src\\* dest\\",                                  "Copy directory recursively", "File Ops"},
        {"MOVE-FILE",           "Move-Item oldname.txt newname.txt",                                 "Rename / move file", "File Ops"},
        {"REMOVE-RECURSE",      "Remove-Item -Recurse -Force build\\",                               "Force-delete directory", "File Ops"},
        {"NEW-DIR",             "New-Item -ItemType Directory logs",                                 "Create directory", "File Ops"},
        {"NEW-FILE",            "New-Item -ItemType File empty.txt",                                 "Create empty file", "File Ops"},
        {"REMOVE-TMP",          "Get-ChildItem -Recurse -Filter *.tmp | Remove-Item -Force",         "Delete temp files", "File Ops"},
        {"ZIP-SRC",             "Compress-Archive -Path src\\* -DestinationPath src.zip",            "Zip source folder", "File Ops"},
        {"UNZIP",               "Expand-Archive src.zip -DestinationPath src_unzipped",              "Extract zip archive", "File Ops"},
        {"UNBLOCK-FILES",       "Get-ChildItem -Recurse | Unblock-File",                             "Unblock downloaded files", "File Ops"},

        // ────────────────────────────────────────────────
        // Cleaning / Build Artifacts
        // ────────────────────────────────────────────────
        {"CLEAN-BIN-OBJ",       "Get-ChildItem -Recurse -Filter bin,obj | Remove-Item -Recurse -Force", "Remove bin & obj folders", "Cleaning"},
        {"CLEAN-O-PDB",         "Get-ChildItem -Recurse -Filter *.o,*.pdb | Remove-Item",            "Remove object/debug files", "Cleaning"},
        {"CLEAN-CACHE",         "Get-ChildItem -Recurse -Filter *.cache | Remove-Item",              "Remove cache folders", "Cleaning"},
        {"CLEAN-TMP",           "Get-ChildItem -Recurse -Filter *.tmp | Remove-Item",                "Remove temporary files", "Cleaning"},
        {"CLEAN-BAK",           "Get-ChildItem -Recurse -Filter *.bak | Remove-Item",                "Remove backup files", "Cleaning"},
        {"CLEAN-DS-STORE",      "Get-ChildItem -Recurse -Filter .DS_Store | Remove-Item",            "Remove macOS junk", "Cleaning"},
        {"CLEAN-THUMBS",        "Get-ChildItem -Recurse -Filter Thumbs.db | Remove-Item",            "Remove Windows thumbs", "Cleaning"},
        {"CLEAN-PYC",           "Get-ChildItem -Recurse -Filter *.pyc,__pycache__ | Remove-Item -Recurse -Force", "Remove Python cache", "Cleaning"},
        {"CLEAN-GIT-UNTRACKED", "git clean -fd",                                                     "Remove untracked files (git)", "Cleaning"},

        // ────────────────────────────────────────────────
        // Git / Repo Helpers
        // ────────────────────────────────────────────────
        {"GIT-STATUS",          "git status",                                                        "Show working tree status", "Git"},
        {"GIT-DIFF",            "git diff",                                                          "Show unstaged changes", "Git"},
        {"GIT-GREP",            "git grep \"pattern\"",                                              "Search tracked files", "Git"},
        {"GIT-CLEAN",           "git clean -fd",                                                     "Remove untracked files", "Git"},
        {"GIT-BRANCHES",        "git branch -a",                                                     "List all branches", "Git"},
        {"GIT-HEAD",            "git rev-parse HEAD",                                                "Current commit hash", "Git"},
        {"GIT-SHOW-FILE",       "git show HEAD:file.cpp",                                            "Show file at HEAD", "Git"},
        {"GIT-LS-FILES",        "git ls-files",                                                      "List tracked files", "Git"},
        {"GIT-BLAME",           "git blame file.cpp",                                                "Show line-by-line authorship", "Git"},
        {"GIT-LOG-GRAPH",       "git log --oneline --graph --decorate",                             "Pretty commit graph", "Git"},

        // ────────────────────────────────────────────────
        // Python Specific
        // ────────────────────────────────────────────────
        {"PY-VENV-CREATE",      "python -m venv .venv",                                              "Create virtual environment", "Python"},
        {"PY-VENV-ACTIVATE",    ".\\.venv\\Scripts\\activate",                                       "Activate venv (Windows)", "Python"},
        {"PY-VENV-REMOVE",      "Remove-Item -Recurse -Force .venv",                                 "Delete virtual environment", "Python"},
        {"PY-PIP-INSTALL",      "pip install -r requirements.txt",                                   "Install dependencies", "Python"},
        {"PY-PIP-FREEZE",       "pip freeze > requirements.txt",                                     "Export installed packages", "Python"},
        {"PY-PIP-UPGRADE",      "python -m pip install --upgrade pip",                               "Upgrade pip itself", "Python"},
        {"PY-HTTP-SERVER",      "python -m http.server 8000",                                        "Start simple HTTP server", "Python"},
        {"PY-UNITTEST",         "python -m unittest discover",                                       "Discover & run tests", "Python"},
        {"PY-PROFILE",          "python -m cProfile script.py",                                      "Profile script execution", "Python"},
        {"PY-BLACK",            "black .",                                                           "Auto-format code", "Python"},
        {"PY-RUFF",             "ruff check .",                                                      "Fast linting", "Python"},
        {"PY-FLAKE8",           "flake8 .",                                                          "Linting with flake8", "Python"},
        {"PY-ISORT",            "isort .",                                                           "Sort imports", "Python"},
        {"PY-BUILD",            "python -m build",                                                   "Build package", "Python"},

        // ────────────────────────────────────────────────
        // C++ Specific
        // ────────────────────────────────────────────────
        {"CPP-CL-COMPILE",      "cl /std:c++20 /EHsc file.cpp",                                      "MSVC compile", "C++"},
        {"CPP-GPP-COMPILE",     "g++ -std=c++20 -O2 -Wall -Wextra -o app.exe *.cpp",                 "GCC compile optimized", "C++"},
        {"CPP-CLANG-ASAN",      "clang++ -std=c++20 -fsanitize=address -g *.cpp -o app_asan.exe",    "Clang + AddressSanitizer", "C++"},
        {"CPP-CMAKE-CONFIG",    "cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug",                      "CMake configure debug", "C++"},
        {"CPP-CMAKE-BUILD",     "cmake --build build --config Release",                              "CMake build release", "C++"},
        {"CPP-CLANG-TIDY",      "clang-tidy *.cpp -- -std=c++20",                                    "Static analysis with clang-tidy", "C++"},
        {"CPP-CPPCHECK",        "cppcheck --enable=all --inconclusive .",                            "Extra static analysis", "C++"},

        // ────────────────────────────────────────────────
        // Build & Automation
        // ────────────────────────────────────────────────
        {"BUILD-MSBUILD",       "msbuild project.sln /p:Configuration=Release",                      "MSBuild release", "Build"},
        {"BUILD-CMAKE",         "cmake --build build --config Release",                              "CMake build", "Build"},
        {"BUILD-NINJA",         "ninja -C build",                                                    "Ninja build", "Build"},
        {"BUILD-DOTNET",        "dotnet build",                                                      "dotnet build", "Build"},
        {"TEST-DOTNET",         "dotnet test",                                                       "Run dotnet tests", "Build"},

        // ────────────────────────────────────────────────
        // Process / System Utilities
        // ────────────────────────────────────────────────
        {"PROC-TOP-CPU",        "Get-Process | Sort-Object CPU -Descending",                        "Top CPU processes", "System"},
        {"PROC-KILL",           "Stop-Process -Name \"chrome\"",                                     "Kill process by name", "System"},
        {"ENV-LIST",            "Get-ChildItem Env:",                                                "List environment variables", "System"},
        {"NET-LISTEN",          "Get-NetTCPConnection | Where-Object {$_.State -eq \"Listen\"}",     "Listening ports", "System"},
        {"CLIP-GET",            "Get-Clipboard",                                                     "Read clipboard", "System"},
        {"CLIP-SET",            "\"hello\" | Set-Clipboard",                                         "Write to clipboard", "System"},
    };
    return items;
}

// ────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────

inline const Item* find(std::string_view name_upper) {
    for (const auto& item : catalog()) {
        if (std::string_view(item.name) == name_upper) {
            return &item;
        }
    }
    return nullptr;
}

inline std::vector<const Item*> search(std::string_view token_upper) {
    std::vector<const Item*> matches;
    std::string token(token_upper);
    std::transform(token.begin(), token.end(), token.begin(), ::toupper);

    for (const auto& item : catalog()) {
        std::string n(item.name);
        std::transform(n.begin(), n.end(), n.begin(), ::toupper);

        std::string cat = item.category ? item.category : "";
        std::transform(cat.begin(), cat.end(), cat.begin(), ::toupper);

        if (n.find(token) != std::string::npos ||
            (!cat.empty() && cat.find(token) != std::string::npos)) {
            matches.push_back(&item);
        }
    }
    return matches;
}

inline std::vector<std::string> categories() {
    std::vector<std::string> cats;
    std::set<std::string> seen;
    for (const auto& item : catalog()) {
        if (item.category && seen.insert(item.category).second) {
            cats.push_back(item.category);
        }
    }
    return cats;
}

} // namespace pshell