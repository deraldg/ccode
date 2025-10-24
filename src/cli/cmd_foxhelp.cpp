// src/cli/cmd_foxhelp.cpp
#include "xbase.hpp"
#include "textio.hpp"
#include "foxref.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using xbase::DbArea;

// helper: uppercase copy
static std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static void print_item(const foxref::Item& it) {
    // NAME  [supported/unsupported]
    std::cout << std::left << std::setw(14) << it.name
              << (it.supported ? " " : " (unsupported) ") << "\n";
    //   syntax
    if (it.syntax && *it.syntax)
        std::cout << "  " << it.syntax << "\n";
    //   summary
    if (it.summary && *it.summary)
        std::cout << "  " << it.summary << "\n";
}

static void do_foxhelp(std::istringstream& in) {
    std::string rest;
    std::getline(in, rest);
    rest = textio::trim(rest);

    if (rest.empty()) {
        // no args: list a compact catalog
        std::cout << "FoxPro-style commands (subset):\n";
        for (const auto& it : foxref::catalog()) {
            std::cout << "  " << std::left << std::setw(12) << it.name;
            if (it.supported) {
                std::cout << " - " << it.summary << "\n";
            } else {
                std::cout << " - " << it.summary << " [unsupported]\n";
            }
        }
        std::cout << "Tip: FOXHELP <NAME> for details, e.g. FOXHELP INDEX\n";
        return;
    }

    // single token lookups first
    {
        if (const auto* hit = foxref::find(rest); hit) {
            print_item(*hit);
            return;
        }
    }

    // tokenized search across name/syntax
    auto results = foxref::search(rest);
    if (!results.empty()) {
        std::cout << "Matches for \"" << rest << "\":\n";
        for (const auto* it : results) {
            print_item(*it);
            std::cout << "\n";
        }
        return;
    }

    std::cout << "No help found for: " << rest << "\n";
    std::cout << "Try FOXHELP (no args) to list commands.\n";
}

// Public handlers (what shell.cpp registers)
void cmd_FOXHELP(DbArea& /*A*/, std::istringstream& in) {
    do_foxhelp(in);
}

void cmd_FH(DbArea& /*A*/, std::istringstream& in) {
    do_foxhelp(in);
}
