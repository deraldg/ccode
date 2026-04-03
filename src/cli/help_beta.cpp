#include "help_beta.hpp"
#include "foxref.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

static std::string trim(std::string s) {
    auto notsp = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static const char* status_word(foxref::BetaStatus st) {
    switch (st) {
        case foxref::BetaStatus::DONE:     return "DONE";
        case foxref::BetaStatus::DEFERRED: return "DEFERRED";
        case foxref::BetaStatus::OPEN:     return "OPEN";
        default:                           return "OPEN";
    }
}

static const char* status_mark(foxref::BetaStatus st) {
    switch (st) {
        case foxref::BetaStatus::DONE:     return "[x]";
        case foxref::BetaStatus::DEFERRED: return "[-]";
        case foxref::BetaStatus::OPEN:     return "[ ]";
        default:                           return "[ ]";
    }
}

} // namespace

namespace dottalk::help {

void show_beta(const std::string& term_upper_or_raw) {
    const std::string term = upper(trim(term_upper_or_raw));

    if (!term.empty()) {
        const foxref::BetaItem* it = foxref::beta_find(term);
        if (!it) {
            std::cout << "BETA: unknown checklist id: " << term << "\n";
            std::cout << "Tip: type HELP BETA (no args) to list all items.\n";
            return;
        }

        const auto st = foxref::beta_effective_status(*it);

        std::cout << "BETA CHECKLIST ITEM\n\n";
        std::cout << "  " << it->id << "  (" << (it->area ? it->area : "?") << ")\n";
        std::cout << "  Status: " << status_word(st) << "\n\n";
        std::cout << it->summary << "\n";
        if (it->details && *it->details) {
            std::cout << "\nDETAILS\n";
            std::cout << it->details << "\n";
        }
        return;
    }

    int done = 0, open = 0, defer = 0;
    for (const auto& it : foxref::beta_catalog()) {
        const auto st = foxref::beta_effective_status(it);
        if (st == foxref::BetaStatus::DONE) ++done;
        else if (st == foxref::BetaStatus::DEFERRED) ++defer;
        else ++open;
    }

    std::cout << "BETA CHECKLIST - DotTalk++ (Road to Beta)\n\n";
    std::cout << "DONE: " << done << "   OPEN: " << open << "   DEFERRED: " << defer << "\n\n";

    std::vector<std::string> order;
    std::map<std::string, std::vector<const foxref::BetaItem*>> groups;

    for (const auto& it : foxref::beta_catalog()) {
        const std::string area = it.area ? std::string(it.area) : std::string("Misc");
        if (groups.find(area) == groups.end()) order.push_back(area);
        groups[area].push_back(&it);
    }

    for (const auto& area : order) {
        std::cout << "Area: " << area << "\n";
        for (const auto* it : groups[area]) {
            const auto st = foxref::beta_effective_status(*it);
            std::cout << "  " << status_mark(st) << " " << it->id << "  " << it->summary << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "Usage:\n";
    std::cout << "  HELP BETA            (show full checklist)\n";
    std::cout << "  HELP BETA <ID>       (show details, e.g., HELP BETA BETA-3.1)\n";
}

} // namespace dottalk::help