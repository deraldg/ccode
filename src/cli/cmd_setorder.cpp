// src/cli/cmd_setorder.cpp — SETORDER <n|tag>
//  - 0      => clear order (physical order)
//  - <tag>  => select order by tag name (quoted allowed)
//  - <n>=1+ => (not implemented here) suggest SETINDEX <tag> or STRUCT to find tags

#include "xbase.hpp"
#include "order_state.hpp"   // orderstate::clearOrder(...)
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <iostream>

// The exported function lives in the xindex namespace.
// (There is also a global one-arg wrapper elsewhere, but we use the 2-arg form.)
namespace xindex {
    bool db_index_set_order(const std::string& tag, xbase::DbArea& area);
}

using xbase::DbArea;

namespace {
// --- local utils: use unique names to avoid clashing with textio::trim ---
static std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}
static std::string trim_local(std::string s){
    auto issp=[](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static bool is_quoted(const std::string& s){
    return s.size()>=2 && ((s.front()=='"' && s.back()=='"') || (s.front()=='\'' && s.back()=='\''));
}
static std::string strip_quotes(std::string s){
    if (is_quoted(s)) s = s.substr(1, s.size()-2);
    return s;
}
static bool is_integer(const std::string& s){
    if (s.empty()) return false;
    size_t i = (s[0]=='+'||s[0]=='-') ? 1u : 0u;
    if (i==s.size()) return false;
    for (; i<s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}
} // namespace

void cmd_SETORDER(DbArea& A, std::istringstream& in)
{
    // read the rest of the line verbatim to allow quoted tags
    std::string rest;
    std::getline(in, rest);
    rest = trim_local(rest);

    if (rest.empty()) {
        std::cout << "Usage: SETORDER <n|tag>\n";
        return;
    }

    // numeric argument?
    if (is_integer(rest)) {
        long long n = 0;
        try { n = std::stoll(rest); } catch (...) { n = -1; }

        if (n == 0) {
            try {
                orderstate::clearOrder(A);
                std::cout << "Order cleared (physical).\n";
            } catch (...) {
                std::cout << "SETORDER: failed to clear order.\n";
            }
            return;
        }

        if (n > 0) {
            std::cout << "SETORDER: numeric selection not available here.\n"
                         "Try: STRUCT to list tags, then SETINDEX <tag> or SETORDER <tag>.\n";
            return;
        }

        std::cout << "SETORDER: order number must be >= 0.\n";
        return;
    }

    // otherwise treat as tag (allow quoted)
    std::string tag = strip_quotes(rest);
    if (tag.empty()) {
        std::cout << "SETORDER: empty tag.\n";
        return;
    }

    try {
        if (xindex::db_index_set_order(tag, A)) {
            std::cout << "Active order tag: " << tag << "\n";
        } else {
            std::cout << "SETORDER: unknown tag '" << tag << "'. Try STRUCT to list available tags.\n";
        }
    } catch (...) {
        std::cout << "SETORDER: failed to set tag '" << tag << "'.\n";
    }
}
