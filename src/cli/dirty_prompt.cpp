// src/cli/dirty_prompt.cpp
#include "cli/dirty_prompt.hpp"

#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#include "cli/table_state.hpp"
#include "xbase.hpp"

// Defined in src/cli/cmd_commit.cpp
extern void cmd_COMMIT(xbase::DbArea& A, std::istringstream& in);

namespace dottalk { namespace dirty {

static bool g_suppress_prompts = false;

extern "C" xbase::XBaseEngine* shell_engine();

// Defined in src/cli/cmd_commit.cpp (global function)
void cmd_COMMIT(xbase::DbArea& A, std::istringstream& in);

static inline std::string to_upper_copy(std::string s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = (char)std::toupper((unsigned char)s[i]);
    }
    return s;
}

static inline bool parse_yes_default_no() {
    std::string answer;
    if (!std::getline(std::cin, answer)) return false;

    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!answer.empty() && is_space((unsigned char)answer.front())) answer.erase(answer.begin());
    while (!answer.empty() && is_space((unsigned char)answer.back()))  answer.pop_back();

    for (std::size_t i = 0; i < answer.size(); ++i) {
        answer[i] = (char)std::tolower((unsigned char)answer[i]);
    }

    if (answer.empty()) return false; // default No
    return (answer == "y" || answer == "yes");
}

static inline bool prompt_commit_yn(const std::string& scopeLabel) {
    std::cout << "TABLE: uncommitted changes detected (" << scopeLabel << "). "
              << "COMMIT changes? (y/N) ";
    std::cout.flush();
    return parse_yes_default_no();
}

static bool commit_area0(int area0) {
    xbase::XBaseEngine* eng = nullptr;
    try { eng = shell_engine(); } catch (...) { eng = nullptr; }
    if (!eng) {
        std::cout << "COMMIT: shell engine unavailable.\n";
        return false;
    }

    try {
        xbase::DbArea& A = eng->area(area0);
        std::istringstream empty;

        const bool prev = g_suppress_prompts;
        g_suppress_prompts = true;
        ::cmd_COMMIT(A, empty);   // <-- global call
        g_suppress_prompts = prev;

        if (dottalk::table::is_enabled(area0) && dottalk::table::is_dirty(area0)) {
            std::cout << "COMMIT failed (area still dirty).\n";
            return false;
        }
        return true;
    } catch (...) {
        std::cout << "COMMIT failed (exception).\n";
        return false;
    }
}

static bool commit_all_dirty() {
    xbase::XBaseEngine* eng = nullptr;
    try { eng = shell_engine(); } catch (...) { eng = nullptr; }
    if (!eng) {
        std::cout << "COMMIT: shell engine unavailable.\n";
        return false;
    }

    bool ok = true;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (dottalk::table::is_enabled(i) && dottalk::table::is_dirty(i)) {
            ok = commit_area0(i) && ok;
        }
    }
    return ok;
}

static int area_index_from_ref(xbase::DbArea& areaRef) {
    xbase::XBaseEngine* eng = nullptr;
    try { eng = shell_engine(); } catch (...) { eng = nullptr; }
    if (!eng) return -1;

    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        try {
            if (&(eng->area(i)) == &areaRef) return i;
        } catch (...) {
        }
    }
    return -1;
}

static bool is_quit_like(const std::string& opLabel) {
    const std::string u = to_upper_copy(opLabel);
    return (u == "QUIT" || u == "EXIT");
}

bool maybe_prompt_area(const int area0, const std::string& opLabel) {
    if (g_suppress_prompts) return true;
    if (!dottalk::table::is_enabled(area0) || !dottalk::table::is_dirty(area0)) return true;

    if (!prompt_commit_yn(std::string("area ") + std::to_string(area0))) return false;

    const bool ok = commit_area0(area0);
    if (ok && is_quit_like(opLabel)) g_suppress_prompts = true;
    return ok;
}

bool maybe_prompt_area(xbase::DbArea& areaRef, const char* opLabel) {
    if (g_suppress_prompts) return true;

    const std::string label = opLabel ? std::string(opLabel) : std::string();
    const int idx = area_index_from_ref(areaRef);
    if (idx >= 0) return maybe_prompt_area(idx, label);

    // Fallback: prompt once, commit all dirty.
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (dottalk::table::is_enabled(i) && dottalk::table::is_dirty(i)) {
            if (!prompt_commit_yn("unknown area (index unavailable)")) return false;
            const bool ok = commit_all_dirty();
            if (ok && is_quit_like(label)) g_suppress_prompts = true;
            return ok;
        }
    }
    return true;
}

bool maybe_prompt_all(xbase::XBaseEngine& eng, const std::string& opLabel) {
    (void)eng;

    if (g_suppress_prompts) return true;

    int dirtyCount = 0;
    for (int i = 0; i < xbase::MAX_AREA; ++i) {
        if (dottalk::table::is_enabled(i) && dottalk::table::is_dirty(i)) ++dirtyCount;
    }
    if (dirtyCount == 0) return true;

    if (!prompt_commit_yn(std::string("all areas (") + std::to_string(dirtyCount) + " dirty)")) return false;

    const bool ok = commit_all_dirty();
    if (ok && is_quit_like(opLabel)) g_suppress_prompts = true;
    return ok;
}

bool maybe_prompt_all(xbase::XBaseEngine& eng, const char* opLabel) {
    return maybe_prompt_all(eng, opLabel ? std::string(opLabel) : std::string());
}

}} // namespace dottalk::dirty
