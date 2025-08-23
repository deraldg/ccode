#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include "xbase.hpp"
#include "textio.hpp"
#include "record_view.hpp"

// SEEK <field> [=] <value>  — case-insensitive exact match on display value

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
    const auto F = db.fields(); // if your API is db.table().fields(), use that
    for (int i = 0; i < static_cast<int>(F.size()); ++i) {
        if (textio::ieq(textio::trim(F[i].name), name)) return i;
    }
    return -1;
}

// Show available fields (same UX as INDEX)
static void show_available_fields(xbase::DbArea& db) {
    try {
        const auto F = db.fields();
        if (!F.empty()) {
            std::cout << "Available:\n";
            for (const auto& m : F) std::cout << "  " << textio::trim(m.name) << "\n";
        }
    } catch (...) { /* ignore */ }
}

void cmd_SEEK(xbase::DbArea& db, std::istringstream& iss) {
    // Parse: SEEK <field> [=] <value>
    std::string rest = strip_inline_comment(iss.str());
    rest = maybe_strip_verb(rest, "SEEK");
    if (rest.empty()) { std::cout << "Usage: SEEK <field> <value>\n"; return; }

    std::istringstream args(rest);
    std::string fld;
    if (!(args >> fld)) { std::cout << "Usage: SEEK <field> <value>\n"; return; }

    std::string tail; std::getline(args, tail); // everything after field
    tail = textio::trim(tail);
    if (!tail.empty() && tail.front() == '=') tail.erase(tail.begin()); // allow optional '='
    std::string value = dequote(textio::trim(tail));

    // Resolve field
    const int fi = resolve_field_index(db, fld);
    if (fi < 0) {
        std::cout << "Unknown field: " << fld << "\n";
        show_available_fields(db);
        return;
    }

    // Empty table?
    if (!db.top()) { std::cout << "Empty table.\n"; return; }

    const std::string needle = tolower_copy(textio::trim(value));

    // Linear scan (case-insensitive equality on display value)
    do {
        if (!db.readCurrent()) continue;
        RecordView rv(db);
        std::string cur = textio::rtrim(rv.getString(fi));
        if (tolower_copy(textio::trim(cur)) == needle) {
            std::cout << "Found at " << db.recno() << ".\n";
            return;
        }
    } while (db.skip(1));

 // where the SEEK linear scan fails
    std::cout << "SEEK: Not found.\n";

}
