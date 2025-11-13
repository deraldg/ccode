// src/cli/cmd_replace.cpp — REPLACE by field INDEX or NAME
// Usage:
//   REPLACE <field_index> WITH <value>
//   REPLACE <field_name>  WITH <value>
//
// Behavior:
// - Resolves <field_name> case-insensitively using A.fields() -> vector<FieldDef>{ name, ... }.
// - Falls back to numeric index parsing (1-based) to match DbArea::set(int, std::string).
// - Writes via writeCurrent() and respects SET TALK ON for echo.
// - No UNIQUE-enforcement here (kept minimal and non-invasive).

#include <sstream>
#include <string>
#include <iostream>
#include <cctype>
#include <vector>
#include <algorithm>

#include "xbase.hpp"
#include "cli/settings.hpp"

using cli::Settings;

static std::string to_upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static bool try_parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    for (; i < s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return false;
    try { long long v = std::stoll(s); out = (int)v; return true; }
    catch (...) { return false; }
}

// Resolve a field name (case-insensitive) to a 1-based index using A.fields().
// Returns 0 if not found.
static int resolve_field_index_by_name(xbase::DbArea& A, const std::string& name) {
    try {
        const auto U = to_upper_copy(name);
        const auto defs = A.fields(); // expects std::vector<xbase::FieldDef>
        for (size_t i = 0; i < defs.size(); ++i) {
            std::string n = defs[i].name; // FieldDef{name, type, length, decimals}
            if (to_upper_copy(n) == U) return (int)i + 1; // DbArea uses 1-based indexing
        }
    } catch (...) {
        // If A.fields() isn't available or throws, just treat as "not found".
    }
    return 0;
}

// Extract the remainder of stream (after WITH) as value, trimming a single leading space.
// Keeps inner spaces verbatim.
static std::string read_value(std::istringstream& in) {
    std::string value;
    std::getline(in, value);
    if (!value.empty() && (value.front()==' ' || value.front()=='\t'))
        value.erase(value.begin());
    return value;
}

void cmd_REPLACE(xbase::DbArea& A, std::istringstream& in)
{
    if (!A.isOpen()) { std::cout << "REPLACE: no file open.\n"; return; }

    // Token could be index or field name
    std::string firstTok;
    if (!(in >> firstTok)) { std::cout << "Usage: REPLACE <field_name|field_index> WITH <value>\n"; return; }

    // Optional "FIELD" keyword (tolerated): REPLACE FIELD <name> WITH <value>
    if (to_upper_copy(firstTok) == "FIELD") {
        if (!(in >> firstTok)) { std::cout << "Usage: REPLACE FIELD <field_name> WITH <value>\n"; return; }
    }

    int fldIndex = 0;
    if (!try_parse_int(firstTok, fldIndex)) {
        // Treat as name
        fldIndex = resolve_field_index_by_name(A, firstTok);
        if (fldIndex <= 0) {
            std::cout << "REPLACE: unknown field '" << firstTok << "'.\n";
            return;
        }
    } else {
        if (fldIndex < 1) {
            std::cout << "REPLACE: field index must be >= 1.\n"; return;
        }
    }

    std::string kw;
    if (!(in >> kw) || to_upper_copy(kw) != "WITH") {
        std::cout << "Usage: REPLACE <field_name|field_index> WITH <value>\n"; return;
    }

    const std::string value = read_value(in);

    bool ok = false;
    try {
        ok = A.set(fldIndex, value) && A.writeCurrent();
    } catch (...) { ok = false; }

    if (!ok) { std::cout << "REPLACE: write failed.\n"; return; }

    if (Settings::instance().talk_on.load())
        std::cout << "Replaced field #" << fldIndex << " at rec " << A.recno() << ".\n";
}
