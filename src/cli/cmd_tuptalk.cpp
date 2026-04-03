// src/cli/cmd_tuptalk.cpp
//
// TUPTALK / TT ? tuple-based normalization test harness + live DBF capture.
//
// Commands:
//   TUPTALK                 # show HELP + status
//   TUPTALK RESET
//   TUPTALK ADD <type> <len> [dec] <raw...>
//   TUPTALK LIST
//   TUPTALK NORMALIZE
//   TUPTALK DUMP
//   TUPTALK EXPORT CSV|TSV [<path>]
//   TUPTALK PUSH <fieldName|#>          # capture one field (schema type/len/dec + current record value)
//   TUPTALK PUSH ALL [FILTER <mask>]    # capture all fields; optional mask (e.g., CND)
//   TUPTALK PUSH FILTER <mask>          # alias of: PUSH ALL FILTER <mask>
//   TUPTALK PUSH ROW                    # capture entire fixed-width padded row as one entry
//   TUPTALK HELP
//
// Notes:
// - Field lookup is forgiving: exact (case-insensitive) ? unique prefix ? unique substring.
// - PUSH ROW builds a single fixed-width, schema-aligned row (good for round-trip tests).
// - When printing schema lengths/decimals we cast to int (avoid unsigned-char/control-char issues).

#include "cmd_tuptalk.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "xbase.hpp"
#include "value_normalize.hpp"

using xbase::DbArea;
using namespace util; // normalize_for_compare

namespace {

// A single tuple entry under test.
struct TupEntry {
    char ftype = 0;          // C / N / D / L
    int  flen  = 0;          // field length
    int  fdec  = 0;          // decimals (for N)
    std::string raw;         // raw input slice
    std::optional<std::string> norm;  // last normalized value, if any
};

// Process-wide scratch buffer for TUPTALK.
static std::vector<TupEntry> g_tuptalk;

// --- small helpers ---

static void ltrim(std::string& s) {
    auto it = std::find_if(s.begin(), s.end(),
                           [](unsigned char ch) { return !std::isspace(ch); });
    s.erase(s.begin(), it);
}
static void rtrim(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
}
static std::string upcopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}
static std::string bracket(const std::string& s) { return "[" + s + "]"; }

static std::string csv_quote(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}
static std::string tsv_sanitize(std::string s) {
    for (char& c : s) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return s;
}

static bool is_area_open(const DbArea* a) {
    if (!a) return false;
    try { return a->isOpen() && !a->filename().empty(); }
    catch (...) { return false; }
}

static void print_status() {
    if (g_tuptalk.empty()) {
        std::cout << "TUPTALK: (empty)\n";
    } else {
        std::cout << "TUPTALK: " << g_tuptalk.size()
                  << " entr" << (g_tuptalk.size() == 1 ? "y" : "ies")
                  << " in buffer. Use TUPTALK LIST / NORMALIZE / DUMP / EXPORT.\n";
    }
}

static void print_help() {
    std::cout <<
        "TUPTALK / TT ? tuple normalization harness\n"
        "Syntax:\n"
        "  TUPTALK                 # show this help + status\n"
        "  TUPTALK RESET\n"
        "  TUPTALK ADD <type> <len> [<dec>] <raw...>\n"
        "      type: C|N|D|L (case-insensitive)\n"
        "      len : integer > 0\n"
        "      dec : required for N (>=0), omitted for C/D/L\n"
        "      raw : rest of the line, stored as-typed (quotes allowed)\n"
        "  TUPTALK LIST\n"
        "  TUPTALK NORMALIZE\n"
        "  TUPTALK DUMP\n"
        "  TUPTALK EXPORT CSV|TSV [<path>]\n"
        "      Export: index,type,len,dec,raw,norm (CSV quoted / TSV sanitized)\n"
        "  TUPTALK PUSH <fieldName|#>\n"
        "      Add one entry using the current record?s value and the field?s type/len/dec\n"
        "      Matching: exact(case-insensitive) ? unique prefix ? unique substring\n"
        "  TUPTALK PUSH ALL [FILTER <mask>]\n"
        "      Add entries for all fields; optional <mask> like CND to restrict types\n"
        "  TUPTALK PUSH FILTER <mask>\n"
        "      Alias for: PUSH ALL FILTER <mask>\n"
        "  TUPTALK PUSH ROW\n"
        "      Add one entry that is the entire fixed-width padded row (schema-aligned)\n"
        "  TUPTALK HELP\n"
        "Examples:\n"
        "  TUPTALK PUSH LASTNAME\n"
        "  TUPTALK PUSH 3\n"
        "  TUPTALK PUSH ALL\n"
        "  TUPTALK PUSH ALL FILTER CND\n"
        "  TUPTALK PUSH ROW\n";
}

// Parse: ADD <type> <len> [dec] <raw...>
static void handle_add(std::istringstream& iss) {
    char ftype = 0;
    if (!(iss >> ftype)) {
        std::cout << "TUPTALK ADD: expected field type (C/N/D/L).\n";
        return;
    }
    const char ftypeU = static_cast<char>(std::toupper(static_cast<unsigned char>(ftype)));
    if (ftypeU!='C' && ftypeU!='N' && ftypeU!='D' && ftypeU!='L') {
        std::cout << "TUPTALK ADD: invalid type '" << ftype
                  << "' (must be C, N, D, or L).\n";
        return;
    }

    int flen = 0;
    if (!(iss >> flen)) {
        std::cout << "TUPTALK ADD: expected field length.\n";
        return;
    }
    if (flen <= 0) {
        std::cout << "TUPTALK ADD: length must be > 0.\n";
        return;
    }

    int fdec = 0;
    if (ftypeU=='N') {
        if (!(iss >> fdec)) {
            std::cout << "TUPTALK ADD: expected decimals for NUMERIC field.\n";
            return;
        }
        if (fdec < 0) {
            std::cout << "TUPTALK ADD: decimals must be >= 0.\n";
            return;
        }
    }

    std::string raw;
    std::getline(iss, raw);
    ltrim(raw); // remove single separator after header tokens

    TupEntry e;
    e.ftype = ftypeU;
    e.flen  = flen;
    e.fdec  = fdec;
    e.raw   = std::move(raw);
    e.norm.reset();

    std::size_t idx = g_tuptalk.size();
    g_tuptalk.push_back(std::move(e));

    std::cout << "TUPTALK: added entry #" << idx
              << " (type=" << g_tuptalk.back().ftype
              << " len=" << g_tuptalk.back().flen
              << " dec=" << g_tuptalk.back().fdec << ").\n";
}

static void handle_reset() {
    g_tuptalk.clear();
    std::cout << "TUPTALK: tuple buffer cleared.\n";
}

static void handle_list() {
    std::cout << "TUPTALK LIST (" << g_tuptalk.size() << " entries):\n";
    for (std::size_t i = 0; i < g_tuptalk.size(); ++i) {
        const auto& e = g_tuptalk[i];
        std::cout << "  [" << i << "]"
                  << " type=" << e.ftype
                  << " len=" << e.flen
                  << " dec=" << e.fdec
                  << " raw=" << bracket(e.raw)
                  << "\n";
    }
}

static void handle_normalize() {
    std::cout << "TUPTALK NORMALIZE:\n";
    for (std::size_t i = 0; i < g_tuptalk.size(); ++i) {
        auto& e = g_tuptalk[i];

        std::optional<std::string> norm =
            normalize_for_compare(e.ftype, e.flen, e.fdec, e.raw);

        std::cout << "  [" << i << "]"
                  << " type=" << e.ftype
                  << " len=" << e.flen
                  << " dec=" << e.fdec
                  << "\n"
                  << "       raw : " << bracket(e.raw) << "\n";

        if (norm) {
            std::cout << "       norm: " << bracket(*norm) << "\n";
            e.norm = std::move(norm);
        } else {
            std::cout << "       norm: <invalid input for this field type>\n";
            e.norm.reset();
        }
    }
}

static void handle_dump() {
    std::cout << "TUPTALK DUMP:\n";
    for (std::size_t i = 0; i < g_tuptalk.size(); ++i) {
        const auto& e = g_tuptalk[i];

        std::cout << "  [" << i << "]"
                  << " type=" << e.ftype
                  << " len=" << e.flen
                  << " dec=" << e.fdec
                  << "\n"
                  << "       raw : " << bracket(e.raw) << "\n";

        if (e.norm) {
            std::cout << "       norm: " << bracket(*e.norm) << "\n";
        } else {
            std::cout << "       norm: <not normalized yet>\n";
        }
    }
}

static void handle_export(std::istringstream& iss) {
    std::string fmt;
    if (!(iss >> fmt)) {
        std::cout << "TUPTALK EXPORT: expected format CSV|TSV.\n";
        return;
    }
    for (auto& c : fmt) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    bool as_csv = false, as_tsv = false;
    if (fmt == "CSV") as_csv = true; else if (fmt == "TSV") as_tsv = true;
    else {
        std::cout << "TUPTALK EXPORT: unknown format '" << fmt << "' (use CSV or TSV).\n";
        return;
    }

    std::string path;
    if (!(iss >> path)) {
        path = as_csv ? "tuptalk_export.csv" : "tuptalk_export.tsv";
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cout << "TUPTALK EXPORT: cannot open \"" << path << "\" for write.\n";
        return;
    }

    // Header
    if (as_csv) {
        out << "index,type,len,dec,raw,norm\n";
    } else { // TSV
        out << "index\ttype\tlen\tdec\traw\tnorm\n";
    }

    std::size_t rows = 0;
    for (std::size_t i = 0; i < g_tuptalk.size(); ++i) {
        const auto& e = g_tuptalk[i];
        // Prefer cached norm; compute on-the-fly if absent (no mutation).
        std::optional<std::string> norm = e.norm;
        if (!norm) norm = normalize_for_compare(e.ftype, e.flen, e.fdec, e.raw);

        if (as_csv) {
            out << i << ','
                << csv_quote(std::string(1, e.ftype)) << ','
                << e.flen << ','
                << e.fdec << ','
                << csv_quote(e.raw) << ','
                << csv_quote(norm ? *norm : std::string{}) << '\n';
        } else {
            out << i << '\t'
                << tsv_sanitize(std::string(1, e.ftype)) << '\t'
                << e.flen << '\t'
                << e.fdec << '\t'
                << tsv_sanitize(e.raw) << '\t'
                << tsv_sanitize(norm ? *norm : std::string{}) << '\n';
        }
        ++rows;
    }

    out.close();
    std::cout << "TUPTALK EXPORT: wrote " << rows
              << " row" << (rows==1? "" : "s")
              << " to \"" << path << "\".\n";
}

// --------- DBF capture + fuzzy field lookup + row/filters ----------

struct FieldLookup {
    bool ok = false;
    int  idx = 0;  // 1-based if ok
    xbase::FieldDef def{};
};

static bool parse_int_strict(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* e = nullptr;
    long v = std::strtol(s.c_str(), &e, 10);
    if (e==s.c_str() || *e!='\0') return false;
    if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max()) return false;
    out = static_cast<int>(v);
    return true;
}

static FieldLookup field_lookup(DbArea& area, const std::string& token) {
    FieldLookup r;
    const auto& defs = area.fields();
    if (defs.empty()) return r;

    // numeric?
    int idx = 0;
    if (parse_int_strict(token, idx)) {
        if (idx >= 1 && idx <= static_cast<int>(defs.size())) {
            r.ok = true; r.idx = idx; r.def = defs[(size_t)idx - 1];
            return r;
        }
        return r;
    }

    // name-based matching
    const std::string needle = upcopy(token);

    // 1) exact (case-insensitive)
    for (size_t i=0;i<defs.size();++i) {
        if (upcopy(defs[i].name) == needle) {
            r.ok = true; r.idx = static_cast<int>(i) + 1; r.def = defs[i]; return r;
        }
    }

    // 2) unique prefix
    int prefix_idx = -1;
    for (size_t i=0;i<defs.size();++i) {
        std::string nn = upcopy(defs[i].name);
        if (nn.rfind(needle, 0) == 0) { // starts with
            if (prefix_idx != -1) { prefix_idx = -2; break; } // ambiguous
            prefix_idx = static_cast<int>(i);
        }
    }
    if (prefix_idx >= 0) {
        r.ok = true; r.idx = prefix_idx + 1; r.def = defs[(size_t)prefix_idx]; return r;
    }

    // 3) unique substring
    int substr_idx = -1;
    for (size_t i=0;i<defs.size();++i) {
        std::string nn = upcopy(defs[i].name);
        if (nn.find(needle) != std::string::npos) {
            if (substr_idx != -1) { substr_idx = -2; break; }
            substr_idx = static_cast<int>(i);
        }
    }
    if (substr_idx >= 0) {
        r.ok = true; r.idx = substr_idx + 1; r.def = defs[(size_t)substr_idx]; return r;
    }

    return r;
}

static void push_one(DbArea& area, int one_based_idx) {
    const auto& defs = area.fields();
    if (one_based_idx < 1 || one_based_idx > (int)defs.size()) {
        std::cout << "TUPTALK PUSH: invalid field index " << one_based_idx << ".\n";
        return;
    }
    const auto& f = defs[(size_t)one_based_idx - 1];

    TupEntry e;
    e.ftype = f.type;
    e.flen  = f.length;
    e.fdec  = f.decimals;
    e.raw   = area.get(one_based_idx); // capture raw text
    e.norm.reset();

    std::size_t idx = g_tuptalk.size();
    g_tuptalk.push_back(std::move(e));

    std::cout << "TUPTALK: pushed #" << idx
              << " from field " << one_based_idx
              << " (" << f.name << ", type=" << f.type
              << " len=" << static_cast<int>(f.length)
              << " dec=" << static_cast<int>(f.decimals) << ").\n";
}

static bool mask_includes(char ftype, const std::string& maskU) {
    const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(ftype)));
    return maskU.find(u) != std::string::npos;
}

static size_t push_all(DbArea& area, const std::string* mask /*nullable*/) {
    const auto& defs = area.fields();
    size_t added = 0;
    const std::string maskU = mask ? upcopy(*mask) : std::string{};
    for (int i=1; i <= (int)defs.size(); ++i) {
        if (mask && !mask_includes(defs[(size_t)i-1].type, maskU)) continue;
        push_one(area, i);
        ++added;
    }
    std::cout << "TUPTALK PUSH ALL" << (mask ? " FILTER " + *mask : "") << ": added "
              << added << " entr" << (added==1? "y" : "ies") << ".\n";
    return added;
}

static std::string lpad(const std::string& s, size_t w) {
    if (s.size() >= w) return s.substr(s.size() - w);
    return std::string(w - s.size(), ' ') + s;
}
static std::string rpad(const std::string& s, size_t w) {
    if (s.size() >= w) return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}

// Build entire fixed-width row using schema widths and simple alignment rules.
static void push_row(DbArea& area) {
    const auto& defs = area.fields();
    if (defs.empty()) {
        std::cout << "TUPTALK PUSH ROW: no fields in current area.\n";
        return;
    }
    std::string out;
    out.reserve(256);
    size_t total_len = 0;
    for (int i=1; i <= (int)defs.size(); ++i) {
        const auto& f = defs[(size_t)i-1];
        std::string v = area.get(i);
        rtrim(v); // why: prevent double-padding artifacts

        size_t w = (f.length > 0 ? (size_t)f.length :
                    (f.type=='D' ? (size_t)8 : (f.type=='L' ? (size_t)1 : (size_t)10)));
        bool right_align = (f.type=='N' || f.type=='F' || f.type=='Y');

        out += right_align ? lpad(v, w) : rpad(v, w);
        total_len += w;
    }

    TupEntry e;
    e.ftype = 'C';
    e.flen  = static_cast<int>(total_len);
    e.fdec  = 0;
    e.raw   = std::move(out);
    e.norm.reset();

    std::size_t idx = g_tuptalk.size();
    g_tuptalk.push_back(std::move(e));

    std::cout << "TUPTALK: pushed ROW as #" << idx
              << " (len=" << (int)total_len << ", fields=" << defs.size() << ").\n";
}

static void handle_push(DbArea& area, std::istringstream& iss) {
    if (!is_area_open(&area)) {
        std::cout << "TUPTALK PUSH: no area open. Use USE <table> first.\n";
        return;
    }

    std::string tok;
    if (!(iss >> tok)) {
        std::cout << "TUPTALK PUSH: missing <fieldName|#>, ALL, FILTER <mask>, or ROW.\n";
        return;
    }

    std::string tokU = upcopy(tok);
    if (tokU == "ROW") {
        push_row(area);
        return;
    }

    if (tokU == "FILTER") {
        std::string mask;
        if (!(iss >> mask)) {
            std::cout << "TUPTALK PUSH FILTER: missing <mask>, e.g., CND.\n";
            return;
        }
        push_all(area, &mask);
        return;
    }

    if (tokU == "ALL") {
        // Optional: FILTER <mask>
        std::string maybe, mask;
        std::streampos p = iss.tellg();
        if (iss >> maybe) {
            if (upcopy(maybe) == "FILTER") {
                if (!(iss >> mask)) {
                    std::cout << "TUPTALK PUSH ALL FILTER: missing <mask>.\n";
                    return;
                }
                push_all(area, &mask);
                return;
            } else {
                // not FILTER; rewind and proceed as plain ALL
                iss.clear(); iss.seekg(p);
            }
        }
        push_all(area, nullptr);
        return;
    }

    // Otherwise: treat token as field reference (name or #)
    FieldLookup r = field_lookup(area, tok);
    if (!r.ok) {
        std::cout << "TUPTALK PUSH: field \"" << tok
                  << "\" not found (use name or 1-based index; matching is exact, "
                     "then unique prefix, then unique substring).\n";
        return;
    }
    push_one(area, r.idx);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public CLI entry point
// ---------------------------------------------------------------------------

void cmd_TUPTALK(DbArea& area, std::istringstream& iss) {
    std::string sub;
    if (!(iss >> sub)) {
        print_help();
        print_status();
        return;
    }

    std::transform(sub.begin(), sub.end(), sub.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (sub == "RESET") {
        handle_reset();
    } else if (sub == "ADD") {
        handle_add(iss);
    } else if (sub == "LIST") {
        handle_list();
    } else if (sub == "NORMALIZE") {
        handle_normalize();
    } else if (sub == "DUMP") {
        handle_dump();
    } else if (sub == "EXPORT") {
        handle_export(iss);
    } else if (sub == "PUSH") {
        handle_push(area, iss);
    } else if (sub == "HELP" || sub == "/?" || sub == "-H" || sub == "--HELP") {
        print_help();
    } else {
        std::cout << "TUPTALK: unknown subcommand \"" << sub
                  << "\" (expected RESET, ADD, LIST, NORMALIZE, DUMP, EXPORT, PUSH, HELP).\n";
    }
}



