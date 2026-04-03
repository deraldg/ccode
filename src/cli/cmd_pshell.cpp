// cmd_pshell.cpp
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <map>

#include "pshell_ref.hpp"

namespace {

std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

void print_item(const pshell::Item& it, bool verbose = true) {
    std::cout << it.name << "\n";
    std::cout << "  " << it.syntax << "\n";
    std::cout << "  " << it.summary << "\n";
    if (verbose) std::cout << "\n";
}

} // anonymous namespace

void show_pshell_help(const std::string& arg) {
    std::string term = to_upper(arg);

    if (term.empty()) {
        std::cout << "POWERSHELL / PSHELL REFERENCE\n\n"
                  << "Common developer one-liners (grouped)\n\n";

        std::map<std::string, std::vector<const pshell::Item*>> grouped;
        for (const auto& item : pshell::catalog()) {
            std::string cat = item.category ? item.category : "Uncategorized";
            grouped[cat].push_back(&item);
        }

        for (const auto& [cat, items] : grouped) {
            std::cout << "=== " << cat << " ===\n";
            for (const auto* it : items) {
                std::cout << std::left << std::setw(28) << it->name
                          << it->summary << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "Usage:\n"
                  << "  PSHELL                     → this grouped list\n"
                  << "  PSHELL PYTHON              → only Python commands\n"
                  << "  PSHELL PY-VENV-CREATE      → show details\n"
                  << "  PSHELL CLEAN*              → search cleaning commands\n"
                  << "  HELP PS LIST-CATEGORIES    → show category names\n"
                  << "  HELP PS <term>             → same as PSHELL <term>\n\n";
        return;
    }

    if (term == "LIST" || term == "LIST-CATEGORIES") {
        std::cout << "PSHELL Categories:\n\n";
        auto cats = pshell::categories();
        for (const auto& cat : cats) {
            std::cout << "  " << cat << "\n";
        }
        std::cout << "\nExample: HELP PS PYTHON\n";
        return;
    }

    // Group filter (e.g. HELP PS PYTHON)
    bool is_group = false;
    std::string group_upper = term;
    for (const auto& item : pshell::catalog()) {
        if (item.category && to_upper(item.category).find(group_upper) != std::string::npos) {
            if (!is_group) {
                std::cout << "=== " << item.category << " ===\n\n";
                is_group = true;
            }
            std::cout << std::left << std::setw(28) << item.name
                      << item.summary << "\n";
        }
    }
    if (is_group) return;

    // Exact match
    if (const auto* item = pshell::find(term)) {
        print_item(*item, true);
        return;
    }

    // Partial / contains search
    auto matches = pshell::search(term);
    if (!matches.empty()) {
        std::cout << "Matching PowerShell helpers:\n\n";
        for (const auto* m : matches) {
            print_item(*m, false);
            std::cout << "\n";
        }
        return;
    }

    std::cout << "No match for: " << term << "\n"
              << "Try: PSHELL, HELP PS LIST-CATEGORIES, or HELP PS <category>\n";
}