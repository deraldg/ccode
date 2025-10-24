// cmd_find.cpp
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include "xbase.hpp"
#include "textio.hpp"
#include "record_view.hpp"

//#define DOTTALK_DEBUG_FIND 1
#if defined(DOTTALK_DEBUG_FIND)
  #define DF(x) do { x; } while(0)
#else
  #define DF(x) do {} while(0)
#endif

// Remove trailing '#' comments that are outside quotes.
static std::string strip_inline_comment(std::string s) {
    bool in_single = false, in_double = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"'  && !in_single) in_double = !in_double;
        else if (c == '\'' && !in_double) in_single = !in_single;
        else if (c == '#' && !in_single && !in_double) { s.resize(i); break; }
    }
    return textio::trim(std::move(s));
}

// If the full command slipped through, remove the leading verb (case-insensitive).
static std::string maybe_strip_verb(std::string s, const char* verb) {
    s = textio::trim(std::move(s));
    if (s.empty()) return s;
    std::istringstream ss(s);
    std::string first; ss >> first;
    if (textio::ieq(first, verb)) {
        std::string tail; std::getline(ss, tail);
        return textio::trim(tail);
    }
    return s;
}

// Dequote "..." or '...'
static std::string dequote(std::string s) {
    s = textio::trim(std::move(s));
    if (s.size() >= 2) {
        char a = s.front(), b = s.back();
        if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
            s.erase(s.begin());
            s.pop_back();
        }
    }
    return s;
}

static std::string tolower_copy(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Case-insensitive, trim-aware field resolver
static int resolve_field_index(xbase::DbArea& db, const std::string& nameIn) {
    const std::string name = textio::trim(nameIn);
    const auto F = db.fields();
    for (int i = 0; i < static_cast<int>(F.size()); ++i) {
        if (textio::ieq(textio::trim(F[i].name), name)) return i;
    }
    return -1;
}

static void show_available_fields(xbase::DbArea& db) {
    try {
        const auto F = db.fields();
        if (!F.empty()) {
            std::cout << "Available:\n";
            for (const auto& m : F) std::cout << "  " << textio::trim(m.name) << "\n";
        }
    } catch (...) { /* ignore */ }
}

// FIND <field> [=] <needle>  — case-insensitive contains() on display value
void cmd_FIND(xbase::DbArea& db, std::istringstream& iss) {
    // We may get the full line in iss; strip verb/comments robustly.
    std::string rest = strip_inline_comment(iss.str());
    rest = maybe_strip_verb(rest, "FIND");

    if (rest.empty()) { std::cout << "Usage: FIND <field> <needle>\n"; return; }

    std::istringstream args(rest);
    std::string fld;
    if (!(args >> fld)) { std::cout << "Usage: FIND <field> <needle>\n"; return; }

    std::string tail; std::getline(args, tail); // everything after field
    tail = textio::trim(tail);
    if (!tail.empty() && tail.front() == '=') tail.erase(tail.begin()); // optional '='
    std::string needle = dequote(textio::trim(tail));

    // Resolve field
    const int fi = resolve_field_index(db, fld);
    if (fi < 0) {
        std::cout << "Unknown field: " << fld << "\n";
        show_available_fields(db);
        return;
    }

    if (!db.top()) { std::cout << "Empty table.\n"; return; }

    const auto fields = db.fields();
    DF(std::cout << "[dbg] fi=" << fi
                 << " name=[" << textio::trim(fields[fi].name) << "]"
                 << " type=" << fields[fi].type << "\n");

    // Fail fast if needle is empty (otherwise we never match)
    const std::string needle_lc = tolower_copy(textio::trim(needle));
    if (needle_lc.empty()) { std::cout << "Usage: FIND <field> <needle>\n"; return; }

    int hits = 0;

    // Linear scan: print "<recno>: <value>" for each match (substring, case-insensitive)
    do {
        if (!db.readCurrent()) continue;

        // IMPORTANT: declare RecordView exactly once per iteration (no duplicates)
        RecordView rv(db);
        std::string cur = textio::rtrim(rv.getString(fi));
        const std::string cur_trim = textio::trim(cur);
        const std::string cur_lc   = tolower_copy(cur_trim);

        DF(if (db.recno() <= 5) {
            std::cout << "[dbg] rec=" << db.recno()
                      << " cur=[" << cur_trim << "]"
                      << " needle=[" << needle << "]\n";
        });

        if (cur_lc.find(needle_lc) != std::string::npos) {
            std::cout << db.recno() << ": " << cur_trim << "\n";
            ++hits;
        }
    } while (db.skip(1));

    if (!hits) std::cout << "Not found.\n";
}
