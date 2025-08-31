// src/cli/cmd_replace_multi.cpp
//
// Transactional multi-field REPLACE with centralized validation.
// - Validates each field against schema and DbArea::trySet (dry-run)
// - Normalizes/quotes values per FoxPro-ish rules (C/M quoted, N/L/D unquoted where appropriate)
// - Builds ONE REPLACE statement and invokes existing cmd_REPLACE
// - On failure, returns false with an explanatory error string

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "xbase.hpp"
#include "command_registry.hpp"

// Bring in your single-field CLI handler so we can reuse it:
extern void cmd_REPLACE(xbase::DbArea&, std::istringstream&);

// Also notify indexes after mutation (wired in your tree):
void order_notify_mutation(xbase::DbArea&) noexcept;

// --- From your bt code ---
struct FieldUpdate { std::string name; std::string value; };

// Small helpers
static inline std::string up(const std::string& s) {
    std::string t; t.reserve(s.size());
    for (unsigned char c : s) t.push_back((char)std::toupper(c));
    return t;
}
static inline void rtrim(std::string& s) {
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
}
static inline std::string trimmed(std::string s) { rtrim(s); return s; }

static std::string escape_quotes(const std::string& in) {
    std::string out; out.reserve(in.size()*2);
    for (char c : in) {
        if (c == '"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

// Normalize DATE: accept YYYYMMDD, YYYY-MM-DD, MM/DD/YYYY -> YYYYMMDD (empty on failure)
static std::string normalize_date(const std::string& in) {
    std::string digits;
    for (char c : in) if (std::isdigit((unsigned char)c)) digits.push_back(c);
    if (digits.size() == 8) {
        auto y = digits.substr(0,4), m = digits.substr(4,2), d = digits.substr(6,2);
        int mi = std::atoi(m.c_str()), di = std::atoi(d.c_str());
        if (mi>=1 && mi<=12 && di>=1 && di<=31) return y+m+d; // YYYYMMDD
        // try MMDDYYYY
        auto m2 = digits.substr(0,2), d2 = digits.substr(2,2), y2 = digits.substr(4,4);
        mi = std::atoi(m2.c_str()); di = std::atoi(d2.c_str());
        if (mi>=1 && mi<=12 && di>=1 && di<=31) return y2+m2+d2;
    }
    return {};
}

// Normalize numeric with length/decimals constraints (empty on failure)
static std::string normalize_numeric(const std::string& in, int length, int decimals) {
    std::string s; s.reserve(in.size());
    bool seen_dot = false;
    for (char c : in) {
        if ((c=='-' && s.empty()) || std::isdigit((unsigned char)c)) { s.push_back(c); continue; }
        if (decimals > 0 && c == '.' && !seen_dot) { seen_dot = true; s.push_back('.'); continue; }
        if (std::isspace((unsigned char)c)) continue;
        return {};
    }
    if (s.empty() || s=="-" || s==".") return {};
    if (decimals == 0) {
        auto dot = s.find('.');
        if (dot != std::string::npos) s.erase(dot);
    } else {
        auto dot = s.find('.');
        if (dot != std::string::npos) {
            size_t frac = s.size() - dot - 1;
            if ((int)frac > decimals) s.erase(dot + 1 + (size_t)decimals);
        }
    }
    if ((int)s.size() > length) return {};
    return s;
}

static std::string normalize_logical(const std::string& in) {
    if (in.empty()) return {};
    char c = (char)std::toupper((unsigned char)in[0]);
    if (c=='T' || c=='Y' || c=='1') return "T";
    if (c=='F' || c=='N' || c=='0') return "F";
    return {};
}

static int field_index_by_name(const std::vector<xbase::FieldDef>& F, const std::string& nameU) {
    for (size_t i=0;i<F.size();++i) if (up(F[i].name)==nameU) return (int)i;
    return -1;
}

// Public entrypoint
bool cmd_REPLACE_MULTI(xbase::DbArea& area,
                       const std::vector<FieldUpdate>& updates,
                       std::string* error)
{
    if (error) error->clear();
    if (!area.isOpen()) { if (error) *error = "No table open."; return false; }
    if (updates.empty()) return true;

    // Make a copy we can validate/normalize
    struct Item { int idx0; std::string name; char type; int len; int dec; std::string normalized; };
    std::vector<Item> items; items.reserve(updates.size());

    const auto& F = area.fields();
    // 1) map fields + per-type normalize
    for (const auto& u : updates) {
        const std::string nameU = up(trimmed(u.name));
        int idx0 = field_index_by_name(F, nameU);
        if (idx0 < 0) { if (error) *error = "Unknown field: " + u.name; return false; }
        const auto& fd = F[(size_t)idx0];

        std::string norm;
        switch (std::toupper((unsigned char)fd.type)) {
            case 'C': // char (free text) — trim right only; actual width will be enforced by trySet or engine
            case 'M': // memo — allow any text; engine handles sidecar
                norm = u.value; break;
            case 'D': {
                norm = normalize_date(u.value);
                if (norm.empty()) { if (error) *error = "Invalid date for " + fd.name + ": " + u.value; return false; }
            } break;
            case 'N': {
                norm = normalize_numeric(u.value, fd.length, fd.decimals);
                if (norm.empty()) { if (error) *error = "Invalid number for " + fd.name + ": " + u.value; return false; }
            } break;
            case 'L': {
                norm = normalize_logical(u.value);
                if (norm.empty()) { if (error) *error = "Invalid logical for " + fd.name + ": " + u.value; return false; }
            } break;
            default: {
                if (error) *error = "Unsupported field type for " + fd.name + ": " + std::string(1,fd.type);
                return false;
            }
        }

        // 2) Dry-run validation against engine (1-based index for API)
        if (!area.trySet(idx0 + 1, norm)) {
            if (error) *error = "Value rejected by engine for " + fd.name + ".";
            return false;
        }

        items.push_back({idx0, fd.name, fd.type, (int)fd.length, (int)fd.decimals, std::move(norm)});
    }

    // 3) Build ONE REPLACE command with proper quoting per type
    auto payload_for = [](char t, const std::string& v)->std::string {
        switch (std::toupper((unsigned char)t)) {
            case 'N': return v;                 // unquoted
            case 'L': return v;                 // T/F unquoted
            case 'D': return "\"" + v + "\"";   // store as YYYYMMDD in quotes
            case 'C':
            case 'M':
            default:  return "\"" + escape_quotes(v) + "\"";
        }
    };

    std::ostringstream cmd;
    cmd << "REPLACE ";
    for (size_t i=0;i<items.size();++i) {
        if (i) cmd << ", ";
        cmd << items[i].name << " WITH " << payload_for(items[i].type, items[i].normalized);
    }

    // 4) Run existing REPLACE handler and refresh indexes
    std::istringstream iss(cmd.str());
    cmd_REPLACE(area, iss);
    order_notify_mutation(area);

    return true;
}
