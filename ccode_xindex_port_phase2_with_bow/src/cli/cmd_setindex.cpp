#include "xbase.hpp"
#include "textio.hpp"
#include "xindex/index_manager.hpp"
#include <sstream>
#include <string>
#include <cctype>

static std::string up(std::string s) { for (auto& c: s) c = (char)std::toupper((unsigned char)c); return s; }

void cmd_SETINDEX(xbase::DbArea& area, std::istringstream& iss) {
    if (!area.is_open()) { textio::print("No table open.\n"); return; }
    std::string tok;
    if (!(iss >> tok)) { textio::print("SET INDEX TO <tag|OFF>\n"); return; }
    if (up(tok)=="TO") {
        if (!(iss >> tok)) { textio::print("SET INDEX TO <tag|OFF>\n"); return; }
    }
    if (up(tok)=="OFF") {
        area.idx()->clear_active();
        textio::print("Order: natural.\n"); return;
    }
    if (!area.idx()->set_active(tok)) {
        textio::printf("No such tag: %s\n", tok.c_str()); return;
    }
    textio::printf("Order set to TAG %s.\n", tok.c_str());
}
