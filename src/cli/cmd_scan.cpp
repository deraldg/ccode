// src/cli/cmd_scan.cpp — SCAN / ENDSCAN with LIST-like FOR and optional WHILE.
//
// Behavior:
// - Traversal matches LIST:
//     * If an index is active: walk the entire order honoring ASCEND/DESCEND.
//     * Else: physical order TOP -> EOF.
// - FOR is parsed as <fld> <op> <value...> and evaluated via predicates::eval
//   (same as LIST). We dequote the value so both bare and quoted strings work.
// - WHILE is an early-exit guard evaluated before each row via predx::eval_expr.
//
// Important change vs previous attempts:
// - This file now includes its *own* index loader (byte-identical logic to LIST),
//   avoiding the '1 recno' issue from a mismatched/hidden definition.

#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <optional>
#include <tuple>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <cstdint>

#include "xbase.hpp"
#include "textio.hpp"
#include "command_registry.hpp"
#include "scan_state.hpp"
#include "predicates.hpp"      // predicates::eval(DbArea&, fld, op, val)
#include "predicate_eval.hpp"  // predx::eval_expr(DbArea&, expr)
#include "order_state.hpp"     // orderstate::{hasOrder, orderName, isAscending}

using xbase::DbArea;

namespace { // ------------------- local helpers & index loader -------------------

// ---- small text helpers ----
static std::string strip_inline_comment(std::string s) {
    bool in_s=false, in_d=false;
    for (size_t i=0; i+1<s.size(); ++i) {
        char c=s[i], n=s[i+1];
        if (!in_s && c=='"')  { in_d=!in_d; continue; }
        if (!in_d && c=='\'') { in_s=!in_s; continue; }
        if (!in_s && !in_d && c=='&' && n=='&') { s.erase(i); break; }
    }
    return textio::rtrim(s);
}

static std::optional<size_t> kw_pos_ci(const std::string& s, const std::string& kw) {
    bool in_s=false, in_d=false;
    const std::string UP = textio::up(kw);
    for (size_t i=0; i<s.size(); ++i) {
        char c=s[i];
        if (!in_s && c=='"')  { in_d=!in_d; continue; }
        if (!in_d && c=='\'') { in_s=!in_s; continue; }
        if (in_s || in_d) continue;
        if (i+kw.size() <= s.size()) {
            bool m=true;
            for (size_t j=0;j<kw.size();++j) {
                if (std::toupper((unsigned char)s[i+j]) != UP[j]) { m=false; break; }
            }
            if (m) {
                if (i>0 && std::isalnum((unsigned char)s[i-1])) continue;
                size_t r=i+kw.size();
                if (r<s.size() && std::isalnum((unsigned char)s[r])) continue;
                return i;
            }
        }
    }
    return std::nullopt;
}

static std::string slice_after_kw(const std::string& s, size_t pos) {
    size_t i=pos;
    while (i<s.size() && !std::isspace((unsigned char)s[i])) ++i;
    while (i<s.size() &&  std::isspace((unsigned char)s[i])) ++i;
    return (i<s.size()) ? textio::trim(s.substr(i)) : std::string{};
}

// LIST-style FOR: "<fld> <op> <value...>"
static std::optional<std::tuple<std::string,std::string,std::string>>
parse_for_triplet(const std::string& seg) {
    std::istringstream iss(seg);
    std::string fld, op;
    if (!(iss >> fld)) return std::nullopt;
    if (!(iss >> op))  return std::nullopt;
    std::string val; std::getline(iss, val);
    val = textio::trim(val);
    if (val.empty()) return std::nullopt;
    return std::make_tuple(fld, op, val);
}

// Dequote "..." / '...' so SCAN accepts either quoted or bare string literals.
static std::string dequote(std::string s) {
    s = textio::trim(s);
    if (s.size() >= 2) {
        char a=s.front(), b=s.back();
        if ((a=='"' && b=='"') || (a=='\'' && b=='\'')) s = s.substr(1, s.size()-2);
    }
    return s;
}

// ---- little-endian readers (byte-identical to LIST) ----
static bool rd_u16(std::istream& in, uint16_t& v) {
    unsigned char b[2];
    if (!in.read(reinterpret_cast<char*>(b), 2)) return false;
    v = static_cast<uint16_t>(b[0] | (b[1] << 8));
    return true;
}
static bool rd_u32(std::istream& in, uint32_t& v) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    v = static_cast<uint32_t>(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
    return true;
}

// ---- EXACT copy of LIST’s loader, under a private name ----
static bool scan_load_inx_recnos(const std::string& path, int32_t maxRecno,
                                 std::vector<uint32_t>& out, std::string* err)
{
    namespace fs = std::filesystem;
    out.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = "cannot open"; return false; }

    auto valid_rn = [&](uint32_t rn){ return rn >= 1u && rn <= static_cast<uint32_t>(maxRecno); };

    // -------------- Try v1: "1INX" magic --------------
    {
        f.clear(); f.seekg(0);
        char magic[4]{};
        if (f.read(magic, 4)) {
            if (std::string(magic, 4) == "1INX") {
                uint16_t skip=0, nameLen=0;
                if (!rd_u16(f, skip) || !rd_u16(f, nameLen)) { if (err) *err = "short header"; return false; }
                if (nameLen > 8192) { if (err) *err = "name too long"; return false; }
                f.seekg(nameLen, std::ios::cur);
                uint32_t count=0;
                if (!rd_u32(f, count)) { if (err) *err = "short count"; return false; }

                out.reserve(count);
                for (uint32_t i = 0; i < count; ++i) {
                    uint16_t klen=0;
                    if (!rd_u16(f, klen)) { if (err) *err = "short entry (klen)"; return false; }
                    f.seekg(klen, std::ios::cur);
                    uint32_t rn=0;
                    if (!rd_u32(f, rn)) { if (err) *err = "short entry (recno)"; return false; }
                    if (!valid_rn(rn)) continue;
                    out.push_back(rn);
                }
                if (!out.empty()) return true;
                // if empty, try other formats before failing
            }
        }
    }

    // -------------- Try v0a: [u16 nameLen][name][u32 count][(u16 klen)(key)(u32 rn)] --------------
    {
        f.clear(); f.seekg(0);
        uint16_t nameLen=0;
        if (rd_u16(f, nameLen) && nameLen <= 8192) {
            f.seekg(nameLen, std::ios::cur);
            uint32_t count=0;
            if (rd_u32(f, count)) {
                std::vector<uint32_t> tmp; tmp.reserve(count);
                bool ok=true;
                for (uint32_t i=0; i<count; ++i) {
                    uint16_t klen=0; uint32_t rn=0;
                    if (!rd_u16(f, klen)) { ok=false; break; }
                    f.seekg(klen, std::ios::cur);
                    if (!rd_u32(f, rn)) { ok=false; break; }
                    if (!valid_rn(rn)) continue;
                    tmp.push_back(rn);
                }
                if (ok && !tmp.empty()) { out.swap(tmp); return true; }
            }
        }
    }

    // -------------- Try v0b: [u32 count][(u16 klen)(key)(u32 rn)] --------------
    {
        f.clear(); f.seekg(0);
        uint32_t count=0;
        if (rd_u32(f, count) && count < 10'000'000u) {
            std::vector<uint32_t> tmp; tmp.reserve(count);
            bool ok=true;
            for (uint32_t i=0; i<count; ++i) {
                uint16_t klen=0; uint32_t rn=0;
                if (!rd_u16(f, klen)) { ok=false; break; }
                f.seekg(klen, std::ios::cur);
                if (!rd_u32(f, rn)) { ok=false; break; }
                if (!valid_rn(rn)) continue;
                tmp.push_back(rn);
            }
            if (ok && !tmp.empty()) { out.swap(tmp); return true; }
        }
    }

    // -------------- Try v0c: [u32 count][(u32 rn)(u16 klen)(key)] --------------
    {
        f.clear(); f.seekg(0);
        uint32_t count=0;
        if (rd_u32(f, count) && count < 10'000'000u) {
            std::vector<uint32_t> tmp; tmp.reserve(count);
            bool ok=true;
            for (uint32_t i=0; i<count; ++i) {
                uint32_t rn=0; uint16_t klen=0;
                if (!rd_u32(f, rn)) { ok=false; break; }
                if (!rd_u16(f, klen)) { ok=false; break; }
                f.seekg(klen, std::ios::cur);
                if (!valid_rn(rn)) continue;
                tmp.push_back(rn);
            }
            if (ok && !tmp.empty()) { out.swap(tmp); return true; }
        }
    }

    if (err) *err = "unrecognized index format";
    return false;
}

} // namespace (local)

// ----------------- file-local state per scan block -----------------

static std::optional<std::tuple<std::string,std::string,std::string>> s_for_triplet; // fld,op,val
static std::optional<std::string> s_while_expr;

// ----------------- commands -----------------

// SCAN [FOR <fld> <op> <value...>] [WHILE <expr>]
void cmd_SCAN(DbArea&, std::istringstream& iss) {
    auto& st = scanblock::state();
    st.active = true;
    st.lines.clear();
    st.for_expr.reset(); // legacy slot unused

    s_for_triplet.reset();
    s_while_expr.reset();

    std::string rest; std::getline(iss, rest);
    rest = strip_inline_comment(rest);

    if (auto p = kw_pos_ci(rest, "FOR"))   s_for_triplet = parse_for_triplet(slice_after_kw(rest, *p));
    if (auto p = kw_pos_ci(rest, "WHILE")) s_while_expr  = slice_after_kw(rest, *p); // full expr

    const bool hasFor   = s_for_triplet.has_value();
    const bool hasWhile = s_while_expr.has_value();

    std::cout << "SCAN: buffering lines. Type ENDSCAN to execute"
              << (hasFor || hasWhile ? " (" : ".")
              << (hasFor ? "FOR" : "")
              << (hasFor && hasWhile ? ", " : "")
              << (hasWhile ? "WHILE" : "")
              << (hasFor || hasWhile ? " present)." : "")
              << "\n";
}

void cmd_ENDSCAN(DbArea& A, std::istringstream&) {
    auto& st = scanblock::state();
    if (!st.active) { std::cout << "No active SCAN.\n"; return; }
    if (!A.isOpen()) {
        std::cout << "No table open.\n";
        st.active = false; st.lines.clear();
        s_for_triplet.reset(); s_while_expr.reset();
        return;
    }

    auto while_ok = [&](DbArea& db)->bool {
        if (!s_while_expr) return true;
        return predx::eval_expr(db, *s_while_expr);
    };

    auto passes_for = [&](DbArea& db)->bool {
        if (!s_for_triplet) return true;
        auto [fld, op, val] = *s_for_triplet;
        val = dequote(val); // accept quoted or bare
        return predicates::eval(db, fld, op, val);
    };

    long long matched = 0, iters = 0;

    auto run_body_for_current = [&](){
        if (A.isDeleted()) return;   // default skip deleted
        if (!passes_for(A)) return;  // per-row filter

        ++matched;

        for (auto raw : st.lines) {
            raw = textio::trim(strip_inline_comment(raw));
            if (raw.empty()) continue;
            std::istringstream iss(raw);
            std::string cmd; iss >> cmd;
            if (cmd.empty()) continue;

            const std::string U = textio::up(cmd);
            if (U == "SCAN" || U == "ENDSCAN") continue;

            if (!dli::registry().run(A, U, iss)) {
                std::cout << "Unknown command in SCAN: " << cmd << "\n";
            }
        }
        ++iters;
    };

    // === Traverse EXACTLY like LIST ===
    bool usedIndex = false;
    if (orderstate::hasOrder(A)) {
        const std::string inxPath = orderstate::orderName(A);
        std::vector<uint32_t> recnos;
        std::string err;
        if (!scan_load_inx_recnos(inxPath, A.recCount(), recnos, &err)) {
            std::cout << "Failed to open index: " << inxPath << " (" << err << ")\n";
        } else {
            usedIndex = !recnos.empty();
            const bool asc = orderstate::isAscending(A);
            if (asc) {
                for (size_t i = 0; i < recnos.size(); ++i) {
                    if (!A.gotoRec((int32_t)recnos[i]) || !A.readCurrent()) continue;
                    if (!while_ok(A)) break;
                    run_body_for_current();
                }
            } else {
                for (size_t i = recnos.size(); i-- > 0; ) {
                    if (!A.gotoRec((int32_t)recnos[i]) || !A.readCurrent()) continue;
                    if (!while_ok(A)) break;
                    run_body_for_current();
                    if (i == 0) break;
                }
            }
        }
    }

    // Fallback: physical order (TOP -> EOF)
    if (!usedIndex) {
        if (A.top() && A.readCurrent()) {
            do {
                if (!while_ok(A)) break;
                run_body_for_current();
            } while (A.skip(+1) && A.readCurrent());
        }
    }

    st.active = false; st.lines.clear();
    s_for_triplet.reset(); s_while_expr.reset();
    std::cout << "ENDSCAN: " << matched << " match(es), " << iters << " iteration(s).\n";
}

// Self-register
static bool s_reg = [](){
    dli::registry().add("SCAN",    [](DbArea& a, std::istringstream& s){ cmd_SCAN(a,s); });
    dli::registry().add("ENDSCAN", [](DbArea& a, std::istringstream& s){ cmd_ENDSCAN(a,s); });
    return true;
}();
