// ===============================================
// src/cli/cmd_wherecache.cpp
// Developer command: WHERECACHE STATS | CLEAR | CAP <n>
// ===============================================
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>

#include "cli/where_eval_shared.hpp"

static inline std::string up(std::string s){
    for(char& c: s) c = (char)std::toupper((unsigned char)c);
    return s;
}

void cmd_WHERECACHE(std::istringstream& args)
{
    std::string tok;
    if (!(args >> tok)) {
        auto st = where_eval::cache_stats();
        std::cout << "; WHERECACHE ? size=" << st.size << " capacity=" << st.capacity << "\n";
        std::cout << "; usage: WHERECACHE [STATS|CLEAR|CAP <n>]\n";
        return;
    }
    std::string T = up(tok);
    if (T == "STATS") {
        auto st = where_eval::cache_stats();
        std::cout << "; WHERECACHE ? size=" << st.size << " capacity=" << st.capacity << "\n";
    } else if (T == "CLEAR") {
        where_eval::cache_clear();
        auto st = where_eval::cache_stats();
        std::cout << "; WHERECACHE ? cleared (size=" << st.size << ")\n";
    } else if (T == "CAP") {
        size_t n = 0;
        if (!(args >> n)) {
            std::cout << "; WHERECACHE ? CAP requires a positive integer\n";
            return;
        }
        where_eval::cache_set_capacity(n);
        auto st = where_eval::cache_stats();
        std::cout << "; WHERECACHE ? capacity set to " << st.capacity << "\n";
    } else {
        std::cout << "; WHERECACHE ? unknown action '" << tok << "'\n";
        std::cout << "; usage: WHERECACHE [STATS|CLEAR|CAP <n>]\n";
    }
}



