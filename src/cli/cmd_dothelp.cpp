#include "xbase.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include "dotref.hpp"

namespace {
std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

void print_item(const dotref::Item& it, bool verbose = true) {
    std::cout << it.name << "\n";
    std::cout << "  " << it.syntax << "\n";
    std::cout << "  " << it.summary << "\n";
    if (!it.supported) {
        std::cout << "  (documented, but not fully supported yet)\n";
    }
    if (verbose) std::cout << "\n";
}
} // namespace

void show_dot_help(const std::string& arg) {
    std::string term = to_upper(arg);
    if (term.empty()) {
        std::cout << "DOTTALK REFERENCE\n\n"
                  << "Project-native commands and subsystems\n\n";
        for (const auto& item : dotref::catalog()) {
            std::cout << item.name << "\n"
                      << "  " << item.syntax << "\n"
                      << "  " << item.summary << "\n\n";
        }
        std::cout << "Usage:\n"
                  << "  DOTHELP <term>\n"
                  << "  HELP /DOT <term>\n";
        return;
    }
    if (const auto* item = dotref::find(term)) {
        print_item(*item, true);
        return;
    }
    auto matches = dotref::search(term);
    if (!matches.empty()) {
        std::cout << "Matching DotTalk helpers:\n\n";
        for (const auto* m : matches) {
            print_item(*m, false);
            std::cout << "\n";
        }
        return;
    }
    std::cout << "No DotTalk help found for: " << term << "\n"
              << "Try HELP /DOT <term> or plain HELP <term>.\n";
}

void cmd_DOTHELP(xbase::DbArea& /*area*/, std::istringstream& iss) {
    std::string args;
    std::getline(iss >> std::ws, args);
    show_dot_help(args);
}
