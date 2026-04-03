#include "foxref.hpp"
#include "help_beta.hpp"
#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

namespace {

static std::string trim(std::string s)
{
    auto notsp = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

static std::string upper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::toupper(c); });
    return s;
}

static void print_usage()
{
    std::cout << "BETA usage:\n";
    std::cout << "  BETA LIST\n";
    std::cout << "  BETA <ID>\n";
    std::cout << "  BETA DONE <ID>\n";
    std::cout << "  BETA DEFER <ID>\n";
    std::cout << "  BETA OPEN <ID>\n";
    std::cout << "  BETA CLEAR <ID>\n";
    std::cout << "  BETA CLEAR ALL\n";
    std::cout << "  BETA SAVE\n";
    std::cout << "  BETA LOAD\n";
}

} // namespace

void cmd_BETA(xbase::DbArea& area, std::istringstream& args)
{
    (void)area; // currently unused, but keep standard handler signature

    std::string sub;
    args >> sub;
    sub = upper(trim(sub));

    if (sub.empty() || sub == "LIST") {
        dottalk::help::show_beta("");
        return;
    }

    // Detail lookup: BETA BETA-3.1
    if (sub.rfind("BETA-", 0) == 0) {
        dottalk::help::show_beta(sub);
        return;
    }

    if (sub == "DONE" || sub == "DEFER" || sub == "DEFERRED" || sub == "OPEN") {
        std::string id;
        args >> id;
        id = upper(trim(id));

        if (id.empty()) {
            print_usage();
            return;
        }

        const foxref::BetaItem* it = foxref::beta_find(id);
        if (!it) {
            std::cout << "BETA: unknown id: " << id << "\n";
            return;
        }

        if (sub == "DONE") {
            foxref::beta_set_status(it->id, foxref::BetaStatus::DONE);
            std::cout << "BETA " << it->id << " -> DONE\n";
            return;
        }

        if (sub == "DEFER" || sub == "DEFERRED") {
            foxref::beta_set_status(it->id, foxref::BetaStatus::DEFERRED);
            std::cout << "BETA " << it->id << " -> DEFERRED\n";
            return;
        }

        foxref::beta_set_status(it->id, foxref::BetaStatus::OPEN);
        std::cout << "BETA " << it->id << " -> OPEN\n";
        return;
    }

    if (sub == "CLEAR") {
        std::string what;
        args >> what;
        what = upper(trim(what));

        if (what.empty()) {
            print_usage();
            return;
        }

        if (what == "ALL") {
            foxref::beta_clear_all_status_overrides();
            std::cout << "BETA: all runtime overrides cleared\n";
            return;
        }

        const foxref::BetaItem* it = foxref::beta_find(what);
        if (!it) {
            std::cout << "BETA: unknown id: " << what << "\n";
            return;
        }

        foxref::beta_clear_status(it->id);
        std::cout << "BETA " << it->id << " override cleared\n";
        return;
    }

    if (sub == "SAVE") {
        std::string err;
        if (!foxref::beta_save_overrides(&err)) {
            std::cout << "BETA SAVE failed: " << err << "\n";
            return;
        }

        std::cout << "BETA SAVE: wrote " << foxref::beta_default_status_path() << "\n";
        return;
    }

    if (sub == "LOAD") {
        std::string err;
        if (!foxref::beta_load_overrides(&err)) {
            std::cout << "BETA LOAD failed: " << err << "\n";
            return;
        }

        std::cout << "BETA LOAD: read " << foxref::beta_default_status_path() << "\n";
        return;
    }

    print_usage();
}