// src/cli/cmd_list.cpp
#include "xbase.hpp"
#include "predicates.hpp"
#include "textio.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <limits>
#include <cctype>
#include <algorithm>
#include <sstream>

namespace {

// ANSI helpers
constexpr const char* RESET   = "\x1b[0m";
constexpr const char* BG_AMBER= "\x1b[48;5;214m"; // amber/orange; fallback: "\x1b[43m" (yellow)
constexpr const char* BG_RED  = "\x1b[41m";
constexpr const char* FG_WHITE= "\x1b[97m";

inline bool is_uint(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
        [](unsigned char c){ return std::isdigit(c)!=0; });
}

struct Options {
    bool all{false};            // LIST ALL (show deleted too)
    int  limit{20};             // default page size
    bool haveFilter{false};     // LIST [N|ALL] FOR <fld> <op> <value...>
    std::string fld, op, val;
};

Options parse_opts(std::istringstream& iss) {
    Options o{};
    std::string tok;

    // Optional first token: ALL or number
    std::streampos save = iss.tellg();
    if (iss >> tok) {
        if (textio::ieq(tok, "ALL")) {
            o.all = true;
        } else if (is_uint(tok)) {
            o.limit = std::max(0, std::stoi(tok));
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }

    // Optional: FOR <fld> <op> <value...>
    save = iss.tellg();
    std::string forWord;
    if (iss >> forWord) {
        if (textio::ieq(forWord, "FOR")) {
            if (iss >> o.fld >> o.op) {
                std::string rest;
                std::getline(iss, rest);
                o.val = textio::trim(rest);
                o.haveFilter = !o.fld.empty() && !o.op.empty() && !o.val.empty();
            }
        } else {
            iss.clear();
            iss.seekg(save);
        }
    }
    return o;
}

// width = digits(recCount), min 3 so small tables still look nice
int recno_width(const xbase::DbArea& a) {
    int n = std::max(1, a.recCount());
    int w = 0;
    while (n) { n /= 10; ++w; }
    return std::max(3, w);
}

inline void print_del_flag(bool isDel) {
    if (isDel) {
        // deleted: red background + white '*'
        std::cout << BG_RED << FG_WHITE << '*' << RESET;
    } else {
        // not deleted: amber background space
        std::cout << BG_AMBER << ' ' << RESET;
    }
}

void print_header(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    // Columns: [status(1)] [space] [recno(recw)] [space] [fields...]
    std::cout << ' ' << ' ' << std::setw(recw) << "" << " ";
    for (const auto& f : Fs) {
        std::cout << std::left << std::setw(static_cast<int>(f.length)) << f.name << " ";
    }
    std::cout << std::right << "\n";
}

void print_row(const xbase::DbArea& a, int recw) {
    const auto& Fs = a.fields();
    print_del_flag(a.isDeleted());
    std::cout << " " << std::setw(recw) << a.recno() << " ";
    for (int i = 1; i <= static_cast<int>(Fs.size()); ++i) {
        std::string s = a.get(i);
        int w = static_cast<int>(Fs[static_cast<size_t>(i-1)].length);
        if (static_cast<int>(s.size()) > w) s.resize(static_cast<size_t>(w));
        std::cout << std::left << std::setw(w) << s << " ";
    }
    std::cout << std::right << "\n";
}

} // namespace

// Shell entrypoint
void cmd_LIST(xbase::DbArea& a, std::istringstream& iss) {
    if (!a.isOpen()) { std::cout << "No table open.\n"; return; }

    Options opt = parse_opts(iss);
    const int32_t total = a.recCount();
    if (total <= 0) { std::cout << "(empty)\n"; return; }

    // If ALL, always start from the top; else from current (or top if unset)
    if (opt.all) a.top(); else if (a.recno() <= 0) a.top();

    const int recw = recno_width(a);
    print_header(a, recw);

    int printed = 0;
    const int32_t start = opt.all ? 1 : a.recno();
    for (int32_t rn = start; rn <= total; ++rn) {
        if (!a.gotoRec(rn)) break;
        if (!a.readCurrent()) continue;

        // default LIST skips deleted
        if (!opt.all && a.isDeleted()) continue;

        if (opt.haveFilter && !predicates::eval(a, opt.fld, opt.op, opt.val))
            continue;

        print_row(a, recw);
        ++printed;

        if (!opt.all && opt.limit > 0 && printed >= opt.limit) break;
    }

    if (!opt.all) {
        std::cout << printed << " record(s) listed (limit "
                  << opt.limit << "). Use LIST ALL to show more.\n";
    } else {
        std::cout << printed << " record(s) listed.\n";
    }
}
