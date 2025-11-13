// src/cli/cmd_status.cpp — concise STATUS reporter using workareas API
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

#include "xbase.hpp"
#include "workareas.hpp"

// Helpers
static std::string basename(const std::string& path) {
    // crude but sufficient for Windows-style paths shown in logs
    size_t p = path.find_last_of("/\\");
    return (p == std::string::npos) ? path : path.substr(p + 1);
}
static std::string stem_upper(const std::string& fname) {
    // Remove extension and upper-case (to match BUILDING / STUDENTS style)
    size_t dot = fname.find_last_of('.');
    std::string s = (dot == std::string::npos) ? fname : fname.substr(0, dot);
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}
static void print_area_line(std::size_t slot, const xbase::DbArea* a, bool is_current) {
    if (a && a->isOpen()) {
        std::string fn, nm;
        try { fn = a->filename(); } catch (...) {}
        try { nm = a->name(); } catch (...) {}
        const std::string file = fn.empty() ? "" : basename(fn);
        const std::string tab  = !nm.empty() ? nm : stem_upper(file);
        std::cout << "Area " << slot << ": " << (tab.empty() ? "[" + std::to_string((int)slot) + "]" : tab);
        if (!file.empty()) std::cout << "  (" << file << ")";
        if (is_current)   std::cout << " [current]";
        std::cout << "\n";
    } else {
        std::cout << "Area " << slot << ": [" << slot << "] (closed)\n";
    }
}

// STATUS dispatcher
void cmd_STATUS(xbase::DbArea&, std::istringstream& args) {
    std::string mode;
    if (!(args >> mode)) mode.clear();

    // normalize
    std::transform(mode.begin(), mode.end(), mode.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });

    const std::size_t n = workareas::count();
    const std::size_t cur = workareas::current_slot();

    auto print_current = [&](){
        if (cur < n) {
            xbase::DbArea* a = workareas::at(cur);
            print_area_line(cur, a, /*is_current*/true);
        } else {
            std::cout << "Area " << cur << ": [" << cur << "] (closed)\n";
        }
    };

    if (mode.empty() || mode == "used") {
        // Current only
        print_current();
        return;
    }
    if (mode == "open") {
        for (std::size_t i = 0; i < n; ++i) {
            xbase::DbArea* a = workareas::at(i);
            if (a && a->isOpen()) print_area_line(i, a, i == cur);
        }
        return;
    }
    if (mode == "all") {
        for (std::size_t i = 0; i < n; ++i) {
            xbase::DbArea* a = workareas::at(i);
            print_area_line(i, a, i == cur);
        }
        return;
    }

    // Fallback: treat unknown token as current (keeps UX forgiving)
    print_current();
}
