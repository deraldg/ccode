// src/cli/cmd_select.cpp ? SELECT <area#|name>
// Supports selecting by numeric slot (0..N-1) or by name/label (case-insensitive).
// Name matching checks workareas::name(i) and the DBF base name from DbArea::filename().
//
// Output style (matches your UX):
//   Selected area 9.
//   Current area: 9
//     File: <path>  Recs: <count>  Recno: <current>
//
// Deps: workareas.hpp, xbase.hpp

#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <limits>

#include "xbase.hpp"
#include "workareas.hpp"

// Provided by the shell (C linkage there)
extern "C" xbase::XBaseEngine* shell_engine();

// ----------------- helpers -----------------

static std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static std::string base_name_upper_from_cstr(const char* pathLike) {
    if (!pathLike) return {};
    std::string s(pathLike);
    for (char& c : s) if (c == '\\') c = '/';
    if (auto pos = s.find_last_of('/'); pos != std::string::npos) s.erase(0, pos + 1);
    std::string S = to_upper(s);
    const std::string ext = ".DBF";
    if (S.size() >= ext.size() && S.substr(S.size() - ext.size()) == ext) {
        S.erase(S.size() - ext.size());
    }
    return S;
}

static std::string base_name_upper_from_str(const std::string& pathLike) {
    return base_name_upper_from_cstr(pathLike.c_str());
}

static bool try_parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    for (; i < s.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    try {
        long long v = std::stoll(s);
        if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) { return false; }
}

// ----------------- command -----------------

void cmd_SELECT(xbase::DbArea& /*A*/, std::istringstream& iss) {
    xbase::XBaseEngine* eng = shell_engine();
    if (!eng) { std::cout << "SELECT: engine unavailable.\n"; return; }

    std::string arg;
    if (!(iss >> arg) || arg.empty()) {
        const size_t cnt = workareas::count();
        std::cout << "Usage: SELECT <0.." << (cnt ? (int)(cnt - 1) : 0) << " | <name>>\n";
        return;
    }

    // Allow quoted names
    if (arg.size() >= 2 && ((arg.front() == '"' && arg.back() == '"') ||
                            (arg.front() == '\'' && arg.back() == '\''))) {
        arg = arg.substr(1, arg.size() - 2);
    }

    int idx = -1;

    // numeric?
    int nParsed = -1;
    if (try_parse_int(arg, nParsed)) {
        const size_t cnt = workareas::count();
        if (nParsed < 0 || (size_t)nParsed >= cnt) {
            std::cout << "SELECT: out of range (valid 0.." << (cnt ? (int)(cnt - 1) : 0) << ").\n";
            return;
        }
        idx = nParsed;
    } else {
        // name/label (case-insensitive), accept with/without .DBF
        std::string wantU = to_upper(arg);
        std::string wantBase = wantU;
        if (wantBase.size() > 4 && wantBase.substr(wantBase.size() - 4) == ".DBF") {
            wantBase.erase(wantBase.size() - 4);
        }

        for (size_t i = 0; i < workareas::count(); ++i) {
            const char* label = workareas::name(i);
            std::string labU   = to_upper(label ? std::string(label) : std::string());
            std::string labBase= base_name_upper_from_cstr(label);

            if ((!labU.empty() && labU == wantU) || (!labBase.empty() && labBase == wantBase)) {
                idx = (int)i;
                break;
            }

            // Fallback: if open, try the filename() base
            const xbase::DbArea* a = workareas::at(i);
            if (a && a->isOpen()) {
                std::string fileBase = base_name_upper_from_str(a->filename());
                if (!fileBase.empty() && fileBase == wantBase) {
                    idx = (int)i;
                    break;
                }
            }
        }

        if (idx < 0) {
            const size_t cnt = workareas::count();
            std::cout << "SELECT: no area matches '" << arg
                      << "'. Use SELECT <0.." << (cnt ? (int)(cnt - 1) : 0)
                      << "> or a known name.\n";
            return;
        }
    }

    // Perform selection & echo
    eng->selectArea((size_t)idx);
    std::cout << "Selected area " << idx << ".\n";

    const xbase::DbArea* cur = workareas::at((size_t)idx);
    if (cur && cur->isOpen()) {
        std::cout << "Current area: " << idx << "\n"
                  << "  File: " << cur->filename()
                  << "  Recs: " << cur->recCount()
                  << "  Recno: " << cur->recno() << "\n";
    } else {
        std::cout << "Current area: " << idx << "\n";
    }
}



