// VALIDATE UNIQUE FIELD <name> [IGNORE DELETED] [REPORT TO <path>]
//
// Scans current work area and reports duplicates for the given field.

#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <algorithm>

#include "xbase.hpp"
#include "textio.hpp"

using namespace textio;

static inline std::string upcopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static int field_index_by_name(xbase::DbArea& A, const std::string& nameU) {
    // In your engine, A.fields() returns a vector<FieldDef>.
    auto defs = A.fields();
    int idx = 1;
    for (const auto& f : defs) {
        std::string U = f.name;
        for (auto& c : U) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (U == nameU) return idx;
        ++idx;
    }
    return 0;
}

// Normalizes CHAR-like values by trimming trailing spaces.
static inline void rtrim_spaces(std::string& s) {
    while (!s.empty() && (unsigned char)s.back() == ' ') s.pop_back();
}

void cmd_VALIDATE_UNIQUE(xbase::DbArea& A, std::istringstream& in) {
    if (!A.isOpen()) { std::cout << "VALIDATE: No file open.\n"; return; }

    std::string tok1, tok2;
    if (!(in >> tok1 >> tok2)) {
        std::cout << "Usage: VALIDATE UNIQUE FIELD <name> [IGNORE DELETED] [REPORT TO <path>]\n";
        return;
    }
    if (upcopy(tok1) != "UNIQUE" || upcopy(tok2) != "FIELD") {
        std::cout << "Usage: VALIDATE UNIQUE FIELD <name> [IGNORE DELETED] [REPORT TO <path>]\n";
        return;
    }

    std::string fieldName;
    if (!(in >> fieldName)) { std::cout << "VALIDATE: Expected field name.\n"; return; }
    const std::string fieldU = upcopy(fieldName);

    bool ignoreDeleted = false;
    std::string reportPath;

    // Parse optional flags
    std::string w1, w2;
    while (in >> w1) {
        const std::string W = upcopy(w1);
        if (W == "IGNORE") {
            if (!(in >> w2) || upcopy(w2) != "DELETED") {
                std::cout << "VALIDATE: Use 'IGNORE DELETED' exactly.\n"; return;
            }
            ignoreDeleted = true;
        } else if (W == "REPORT") {
            if (!(in >> w2) || upcopy(w2) != "TO") {
                std::cout << "VALIDATE: Use 'REPORT TO <path>'.\n"; return;
            }
            std::string path;
            if (!(in >> std::ws) || !std::getline(in, path) || path.empty()) {
                std::cout << "VALIDATE: Missing report path after 'REPORT TO'.\n"; return;
            }
            size_t p = path.find_first_not_of(' ');
            reportPath = (p == std::string::npos) ? std::string() : path.substr(p);
        } else {
            std::cout << "VALIDATE: Unrecognized option '" << w1 << "'.\n"; return;
        }
    }

    const int idx = field_index_by_name(A, fieldU);
    if (idx <= 0) { std::cout << "VALIDATE: Field not found: " << fieldName << "\n"; return; }

    const int startRec = A.recno();
    const int total = A.recCount();
    if (total <= 0) { std::cout << "VALIDATE: Table is empty.\n"; return; }

    std::unordered_map<std::string, int> firstSeen;
    struct Dup { int recno; std::string value; int first; };
    std::vector<Dup> dups;
    dups.reserve(16);

    for (int r = 1; r <= total; ++r) {
        if (!A.gotoRec(r)) continue;
        if (ignoreDeleted) {
            try { if (A.isDeleted()) continue; } catch (...) {}
        }
        std::string val;
        try {
            // Assuming A.get(int) -> std::string (adjust if needed).
            val = A.get(idx);
        } catch (...) {
            continue;
        }
        rtrim_spaces(val);

        const auto it = firstSeen.find(val);
        if (it == firstSeen.end()) {
            firstSeen.emplace(val, r);
        } else {
            dups.push_back({r, val, it->second});
        }
    }

    if (startRec > 0) A.gotoRec(startRec);

    if (dups.empty()) {
        std::cout << "VALIDATE: OK — field '" << fieldName << "' is unique across "
                  << total << " record(s)" << (ignoreDeleted ? " (ignoring deleted)" : "") << ".\n";
        return;
    }

    std::cout << "VALIDATE: Found " << dups.size() << " duplicate record(s) on field '"
              << fieldName << "'" << (ignoreDeleted ? " (ignoring deleted)" : "") << ".\n";

    const int preview = std::min<int>(5, (int)dups.size());
    for (int i=0; i<preview; ++i) {
        const auto& d = dups[i];
        std::cout << "  dup value='" << d.value << "' at rec " << d.recno
                  << " (first seen at rec " << d.first << ")\n";
    }
    if ((int)dups.size() > preview) {
        std::cout << "  ... and " << (dups.size() - preview) << " more.\n";
    }

    if (!reportPath.empty()) {
        std::ofstream out(reportPath, std::ios::binary);
        if (!out) { std::cout << "VALIDATE: Could not write report: " << reportPath << "\n"; return; }
        out << "recno,value,first_seen\n";
        for (const auto& d : dups) {
            out << d.recno << ",\"" << d.value << "\"," << d.first << "\n";
        }
        std::cout << "Report written: " << reportPath << "\n";
    }
}
