// VALIDATE UNIQUE FIELD <name> [IGNORE DELETED] [REPAIR] [REPORT TO <path>]
//
// Scans current work area and reports duplicates for the given field.
// REPAIR is intended for numeric unique/autokey-style fields:
//   - blank values are assigned new numbers
//   - duplicate values keep the first occurrence
//   - later duplicates are assigned new numbers

#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <algorithm>
#include <limits>

#include "xbase.hpp"
#include "textio.hpp"

using namespace textio;

static inline std::string upcopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

static int field_index_by_name(xbase::DbArea& A, const std::string& nameU) {
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

static inline void rtrim_spaces(std::string& s) {
    while (!s.empty() && (unsigned char)s.back() == ' ') s.pop_back();
}

static inline bool is_blank_value(const std::string& s) {
    return s.empty();
}

static bool parse_long_long_strict(const std::string& s, long long& out) {
    if (s.empty()) return false;

    size_t start = 0;
    if (s[0] == '+' || s[0] == '-') start = 1;
    if (start >= s.size()) return false;

    for (size_t i = start; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    }

    try {
        out = std::stoll(s);
        return true;
    } catch (...) {
        return false;
    }
}

static bool field_is_numericish(xbase::DbArea& A, int idx) {
    const auto defs = A.fields();
    if (idx < 1 || idx > (int)defs.size()) return false;

    const char t = (char)std::toupper((unsigned char)defs[(size_t)(idx - 1)].type);
    return t == 'N' || t == 'I' || t == 'Y' || t == 'F' || t == 'B';
}

static long long compute_max_numeric_value(xbase::DbArea& A, int idx, bool ignoreDeleted) {
    long long mx = std::numeric_limits<long long>::min();
    const int total = A.recCount();
    const int save = A.recno();

    for (int r = 1; r <= total; ++r) {
        if (!A.gotoRec(r)) continue;
        if (ignoreDeleted) {
            try { if (A.isDeleted()) continue; } catch (...) {}
        }

        std::string val;
        try {
            val = A.get(idx);
        } catch (...) {
            continue;
        }

        rtrim_spaces(val);

        long long n = 0;
        if (parse_long_long_strict(val, n)) {
            if (n > mx) mx = n;
        }
    }

    if (save > 0) {
        A.gotoRec(save);
    }

    if (mx == std::numeric_limits<long long>::min())
        return 0;

    return mx;
}

void cmd_VALIDATE_UNIQUE(xbase::DbArea& A, std::istringstream& in) {
    if (!A.isOpen()) {
        std::cout << "VALIDATE: No file open.\n";
        return;
    }

    std::string tok1, tok2;
    if (!(in >> tok1 >> tok2)) {
        std::cout << "Usage: VALIDATE UNIQUE FIELD <name> [IGNORE DELETED] [REPAIR] [REPORT TO <path>]\n";
        return;
    }
    if (upcopy(tok1) != "UNIQUE" || upcopy(tok2) != "FIELD") {
        std::cout << "Usage: VALIDATE UNIQUE FIELD <name> [IGNORE DELETED] [REPAIR] [REPORT TO <path>]\n";
        return;
    }

    std::string fieldName;
    if (!(in >> fieldName)) {
        std::cout << "VALIDATE: Expected field name.\n";
        return;
    }
    const std::string fieldU = upcopy(fieldName);

    bool ignoreDeleted = false;
    bool doRepair = false;
    std::string reportPath;

    std::string w1, w2;
    while (in >> w1) {
        const std::string W = upcopy(w1);

        if (W == "IGNORE") {
            if (!(in >> w2) || upcopy(w2) != "DELETED") {
                std::cout << "VALIDATE: Use 'IGNORE DELETED' exactly.\n";
                return;
            }
            ignoreDeleted = true;
        } else if (W == "REPAIR") {
            doRepair = true;
        } else if (W == "REPORT") {
            if (!(in >> w2) || upcopy(w2) != "TO") {
                std::cout << "VALIDATE: Use 'REPORT TO <path>'.\n";
                return;
            }
            std::string path;
            if (!(in >> std::ws) || !std::getline(in, path) || path.empty()) {
                std::cout << "VALIDATE: Missing report path after 'REPORT TO'.\n";
                return;
            }
            size_t p = path.find_first_not_of(' ');
            reportPath = (p == std::string::npos) ? std::string() : path.substr(p);
        } else {
            std::cout << "VALIDATE: Unrecognized option '" << w1 << "'.\n";
            return;
        }
    }

    const int idx = field_index_by_name(A, fieldU);
    if (idx <= 0) {
        std::cout << "VALIDATE: Field not found: " << fieldName << "\n";
        return;
    }

    if (doRepair && !field_is_numericish(A, idx)) {
        std::cout << "VALIDATE: REPAIR currently supports numeric/autokey-style fields only.\n";
        return;
    }

    const int startRec = A.recno();
    const int total = A.recCount();
    if (total <= 0) {
        std::cout << "VALIDATE: Table is empty.\n";
        return;
    }

    std::unordered_map<std::string, int> firstSeen;
    struct Dup { int recno; std::string value; int first; bool blank; };
    std::vector<Dup> dups;
    dups.reserve(16);

    int blankCount = 0;

    for (int r = 1; r <= total; ++r) {
        if (!A.gotoRec(r)) continue;

        if (ignoreDeleted) {
            try { if (A.isDeleted()) continue; } catch (...) {}
        }

        std::string val;
        try {
            val = A.get(idx);
        } catch (...) {
            continue;
        }

        rtrim_spaces(val);

        if (is_blank_value(val)) {
            ++blankCount;
            dups.push_back({r, val, 0, true});
            continue;
        }

        const auto it = firstSeen.find(val);
        if (it == firstSeen.end()) {
            firstSeen.emplace(val, r);
        } else {
            dups.push_back({r, val, it->second, false});
        }
    }

    int repaired = 0;

    if (doRepair && !dups.empty()) {
        long long nextValue = compute_max_numeric_value(A, idx, ignoreDeleted) + 1;

        for (const auto& d : dups) {
            if (!A.gotoRec(d.recno)) continue;

            if (ignoreDeleted) {
                try { if (A.isDeleted()) continue; } catch (...) {}
            }

            try {
                A.readCurrent();
                A.set(idx, std::to_string(nextValue));
                if (A.writeCurrent()) {
                    ++repaired;
                    ++nextValue;
                }
            } catch (...) {
            }
        }

        // Re-scan after repair so report/output reflects final state.
        firstSeen.clear();
        dups.clear();
        blankCount = 0;

        for (int r = 1; r <= total; ++r) {
            if (!A.gotoRec(r)) continue;

            if (ignoreDeleted) {
                try { if (A.isDeleted()) continue; } catch (...) {}
            }

            std::string val;
            try {
                val = A.get(idx);
            } catch (...) {
                continue;
            }

            rtrim_spaces(val);

            if (is_blank_value(val)) {
                ++blankCount;
                dups.push_back({r, val, 0, true});
                continue;
            }

            const auto it = firstSeen.find(val);
            if (it == firstSeen.end()) {
                firstSeen.emplace(val, r);
            } else {
                dups.push_back({r, val, it->second, false});
            }
        }
    }

    if (startRec > 0) {
        A.gotoRec(startRec);
        try { A.readCurrent(); } catch (...) {}
    }

    if (dups.empty()) {
        std::cout << "VALIDATE: OK - field '" << fieldName << "' is unique across "
                  << total << " record(s)"
                  << (ignoreDeleted ? " (ignoring deleted)" : "")
                  << ".";
        if (doRepair) {
            std::cout << " REPAIR updated " << repaired << " record(s).";
        }
        std::cout << "\n";
        return;
    }

    std::cout << "VALIDATE: Found " << dups.size()
              << " problem record(s) on field '" << fieldName << "'"
              << (ignoreDeleted ? " (ignoring deleted)" : "") << ".\n";

    const int preview = std::min<int>(5, (int)dups.size());
    for (int i = 0; i < preview; ++i) {
        const auto& d = dups[i];
        if (d.blank) {
            std::cout << "  blank value at rec " << d.recno << "\n";
        } else {
            std::cout << "  dup value='" << d.value << "' at rec " << d.recno
                      << " (first seen at rec " << d.first << ")\n";
        }
    }
    if ((int)dups.size() > preview) {
        std::cout << "  ... and " << (dups.size() - preview) << " more.\n";
    }

    if (doRepair) {
        std::cout << "VALIDATE: REPAIR updated " << repaired
                  << " record(s), but " << dups.size()
                  << " problem record(s) remain.\n";
    }

    if (!reportPath.empty()) {
        std::ofstream out(reportPath, std::ios::binary);
        if (!out) {
            std::cout << "VALIDATE: Could not write report: " << reportPath << "\n";
            return;
        }

        out << "recno,value,first_seen,kind\n";
        for (const auto& d : dups) {
            out << d.recno << ",\""
                << d.value << "\","
                << d.first << ","
                << (d.blank ? "blank" : "duplicate")
                << "\n";
        }
        std::cout << "Report written: " << reportPath << "\n";
    }
}