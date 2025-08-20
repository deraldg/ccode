// src/cli/cmd_setorder.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <cctype>
#include "order_state.hpp"


#include "xbase.hpp"
#include "textio.hpp"
#include "order_state.hpp"

using xbase::DbArea;
namespace fs = std::filesystem;

namespace {
    void usage() {
        std::cout
          << "Usage:\n"
          << "  SETORDER                 (show current order)\n"
          << "  SETORDER TO <index>      (attach <index>[.inx])\n"
          << "  SETORDER TO 0            (clear order)\n"
          << "  CLEAR ORDER              (alias to clear)\n";
    }

    static std::string lower(std::string s) {
        for (auto& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

//    static bool normalize_and_check(std::string& path) {
//        if (path.size() < 4 || lower(path.substr(path.size()-4)) != ".inx")            path += ".inx";
//        return fs::exists(path);
//    }

}

namespace {
    bool is_comment_token(const std::string& t) {
        return t.rfind("//", 0) == 0 || t == "&&" || t.rfind("#", 0) == 0;
    }

    static bool normalize_and_check(std::string& path) {
        auto lower = [](std::string s){ for (auto& c: s) c = (char)std::tolower((unsigned char)c); return s; };
        if (path.size() < 4 || lower(path.substr(path.size()-4)) != ".inx")
            path += ".inx";
        namespace fs = std::filesystem;
        try {
            return fs::exists(path);
        } catch (...) {
            return false; // don’t explode on weird paths
        }
    }
}

void cmd_SETORDER(DbArea& A, std::istringstream& S)
{
    // First token after SETORDER (if any)
    std::string tok;
    if (!(S >> tok) || is_comment_token(tok)) {
        // No args -> show current
        if (orderstate::hasOrder(A))
            std::cout << "Current order: " << orderstate::orderName(A) << "\n";
        else
            std::cout << "Current order: (natural)\n";
        return;
    }

    std::string U = textio::up(tok);

    // Support legacy "SETORDER ORDER ..." by swallowing the word ORDER
    if (U == "ORDER") {
        // If nothing else, show current
        if (S.peek() == std::char_traits<char>::eof()) {
            if (orderstate::hasOrder(A))
                std::cout << "Current order: " << orderstate::orderName(A) << "\n";
            else
                std::cout << "Current order: (natural)\n";
            return;
        }
        // Read the next token as if it were the first
        S >> tok;
        U = textio::up(tok);
    }

    // CLEAR / CLEAR ORDER
    if (U == "CLEAR") {
        std::string maybeOrder;
        S >> maybeOrder; // optional "ORDER"
        orderstate::clearOrder(A);
        std::cout << "Order cleared (natural/physical).\n";
        return;
    }

    // SETORDER TO ...
    if (U == "TO") {
        std::string idx;
        std::getline(S, idx);
        idx = textio::trim(idx);

        if (idx.empty()) { usage(); return; }

        std::string UI = textio::up(idx);
        if (UI == "0" || UI == "CLEAR" || UI == "NONE") {
            orderstate::clearOrder(A);
            std::cout << "Order cleared (natural/physical).\n";
            return;
        }

        std::string candidate = idx;
        if (!normalize_and_check(candidate)) {
            std::cout << "SET ORDER: index not found: " << idx << "\n";
            return;
        }
        orderstate::setOrder(A, candidate);
        std::cout << "Order set to: " << orderstate::orderName(A) << "\n";
        return;
    }

    // Shorthand: SETORDER <index>
    // Reconstruct rest of line with the first token included
    {
        std::string tail((std::istreambuf_iterator<char>(S)), {});
        std::string idx = textio::trim(tok + (tail.empty() ? "" : " " + textio::trim(tail)));
        if (idx.empty()) { usage(); return; }

        std::string UI = textio::up(idx);
        if (UI == "0" || UI == "CLEAR" || UI == "NONE") {
            orderstate::clearOrder(A);
            std::cout << "Order cleared (natural/physical).\n";
            return;
        }

        std::string candidate = idx;
        if (!normalize_and_check(candidate)) {
            std::cout << "SET ORDER: index not found: " << idx << "\n";
            return;
        }
        orderstate::setOrder(A, candidate);
        std::cout << "Order set to: " << orderstate::orderName(A) << "\n";
        return;
    }
}
