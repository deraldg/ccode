// src/cli/cmd_scan.cpp - SCAN / ENDSCAN with LIST-like FOR and optional WHILE.

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
#include <cerrno>
#include <cstring>

#include "xbase.hpp"
#include "textio.hpp"
#include "command_registry.hpp"
#include "scan_state.hpp"
#include "predicates.hpp"      // predicates::eval(DbArea&, fld, op, val)
#include "predicate_eval.hpp"  // predx::eval_expr(DbArea&, expr)
#include "order_state.hpp"     // orderstate::{hasOrder, orderName, isAscending}

using xbase::DbArea;

// ----------------- robust registry invoker (uses your existing dli::registry) -----------------

template <typename R>
static inline void invoke_registry_impl(R& reg, const std::string& name, DbArea& A, std::istringstream& args) {
    if constexpr (requires { reg.dispatch(name, A, args); }) {
        reg.dispatch(name, A, args);
    } else if constexpr (requires { reg.call(name, A, args); }) {
        reg.call(name, A, args);
    } else if constexpr (requires { reg.execute(name, A, args); }) {
        reg.execute(name, A, args);
    } else if constexpr (requires { reg.run(name, A, args); }) {
        reg.run(name, A, args);
    } else if constexpr (requires { reg.invoke(name, A, args); }) {
        reg.invoke(name, A, args);
    } else if constexpr (requires { reg.exec(name, A, args); }) {
        reg.exec(name, A, args);
    } else if constexpr (requires { reg(name, A, args); }) {
        reg(name, A, args); // operator()
    } else {
        // No compatible entry point; keep SCAN resilient by doing nothing.
    }
}

static inline void invoke_registry_command(const std::string& name, DbArea& A, std::istringstream& args) {
    auto& reg = dli::registry();   // declared in your command_registry.hpp
    invoke_registry_impl(reg, name, A, args);
}

namespace { // local-only helpers and state

// ----------------- tiny helpers -----------------

static std::string strip_inline_comment(std::string s) {
    // Remove && ... but honor quotes.
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

static std::string slice_after_kw(const std::string& s, size_t kwPos) {
    // Return trimmed substring after the keyword token at kwPos.
    size_t i = kwPos;
    while (i < s.size() && !std::isspace((unsigned char)s[i])) ++i; // skip keyword
    while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;  // skip spaces
    return textio::trim(s.substr(i));
}

static std::tuple<std::string,std::string,std::string> parse_for_triplet(const std::string& rest) {
    // rest is "<fld> <op> <value...>"
    std::istringstream in(rest);
    std::string fld, op;
    if (!(in >> fld) || !(in >> op)) return {"","",""};
    std::string value; std::getline(in, value); value = textio::ltrim(value);
    return {fld, op, value};
}

static std::string dequote(std::string s) {
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

// ---- EXACT copy of LIST's loader, under a private name ----
static bool scan_load_inx_recnos(const std::string& path, int32_t maxRecno,
                                 std::vector<uint32_t>& out, std::string* err)
{
    namespace fs = std::filesystem;
    out.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err) *err = std::strerror(errno);
        return false;
    }

    auto valid_rn = [&](uint32_t rn)->bool {
        return rn >= 1u && rn <= (uint32_t)std::max<int32_t>(1, maxRecno);
    };

    // v1: magic "1INX" + count + recnos
    {
        f.clear(); f.seekg(0);
        char magic[4] = {0,0,0,0};
        if (f.read(magic, 4) && std::string(magic,4) == "1INX") {
            uint32_t count=0; if (!rd_u32(f, count)) return false;
            if (count > 10000000u) return false;
            std::vector<uint32_t> tmp; tmp.reserve(count);
            for (uint32_t i=0; i<count; ++i) {
                uint32_t rn=0; if (!rd_u32(f, rn)) return false;
                if (!valid_rn(rn)) continue;
                tmp.push_back(rn);
            }
            if (!tmp.empty()) { out.swap(tmp); return true; }
        }
    }

    // v0a: [u32 count][u32 recno]*
    {
        f.clear(); f.seekg(0);
        uint32_t count=0;
        if (rd_u32(f, count) && count < 10000000u) {
            std::vector<uint32_t> tmp; tmp.reserve(count);
            bool ok=true;
            for (uint32_t i=0; i<count; ++i) {
                uint32_t rn=0;
                if (!rd_u32(f, rn)) { ok=false; break; }
                if (!valid_rn(rn)) continue;
                tmp.push_back(rn);
            }
            if (ok && !tmp.empty()) { out.swap(tmp); return true; }
        }
    }

    // v0b: [(u16 len)(char key[len])(u32 recno)]*
    {
        f.clear(); f.seekg(0);
        std::vector<uint32_t> tmp;
        bool ok=true;
        while (true) {
            uint16_t klen=0;
            if (!rd_u16(f, klen)) break; // EOF ok
            std::string key; key.resize(klen);
            if (!f.read(&key[0], klen)) { ok=false; break; }
            uint32_t rn=0; if (!rd_u32(f, rn)) { ok=false; break; }
            if (!valid_rn(rn)) continue;
            tmp.push_back(rn);
        }
        if (ok && !tmp.empty()) { out.swap(tmp); return true; }
    }

    // v0c: [u32 count][(u32 rn)(u16 klen)(key)]
    {
        f.clear(); f.seekg(0);
        uint32_t count=0;
        if (rd_u32(f, count) && count < 10000000u) {
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

// ----------------- SCAN buffer state -----------------

static std::optional<std::tuple<std::string,std::string,std::string>> s_for_triplet; // fld,op,val
static std::optional<std::string> s_while_expr;

} // end anonymous namespace

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

    // Parse FOR/WHILE with proper bounding when both are present.
    auto pFor   = kw_pos_ci(rest, "FOR");
    auto pWhile = kw_pos_ci(rest, "WHILE");

    if (pFor) {
        if (pWhile && *pWhile > *pFor) {
            std::string forSeg = textio::trim(rest.substr(*pFor, *pWhile - *pFor));
            s_for_triplet = parse_for_triplet(slice_after_kw(forSeg, 0));
        } else {
            s_for_triplet = parse_for_triplet(slice_after_kw(rest, *pFor));
        }
    }
    if (pWhile) {
        s_while_expr = slice_after_kw(rest, *pWhile);
    }

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

    // Make local copies of the buffered lines; strip inline comments (&&) quote-aware.
    std::vector<std::string> lines; lines.reserve(st.lines.size());
    for (const auto& raw : st.lines) {
        auto L = strip_inline_comment(raw);
        if (!L.empty()) lines.push_back(L);
    }

    // ---- helpers: predicate + WHILE guard ----
    const bool hasFor = s_for_triplet.has_value();
    auto for_ok = [&](DbArea& area)->bool {
        if (!hasFor) return true;
        const auto& t = *s_for_triplet;
        const std::string& fld = std::get<0>(t);
        const std::string& op  = std::get<1>(t);
        std::string val = dequote(std::get<2>(t));
        try { return predicates::eval(area, fld, op, val); }
        catch (...) { return false; }
    };

    const bool hasWhile = s_while_expr.has_value();
    auto while_ok = [&](DbArea& area)->bool {
        if (!hasWhile) return true;
        try { return predx::eval_expr(area, *s_while_expr); }
        catch (...) { return false; }
    };

    // ---- executor for one record ----
    int matched = 0, iters = 0;
    auto run_body_for_current = [&](){
        if (!for_ok(A)) return;
        if (A.isDeleted()) return; // default FoxPro behavior (unless SET DELETED OFF someday)
        ++matched;
        // Execute buffered commands on current record
        for (const auto& L : lines) {
            std::istringstream li(L);
            std::string cmd; if (!(li >> cmd) || cmd.empty()) continue;
            try {
                invoke_registry_command(textio::up(cmd), A, li);
            } catch (...) {
                // Keep SCAN resilient: ignore per-line errors but continue loop.
            }
        }
        ++iters;
    };

    bool usedIndex = false;

    // Prefer active order traversal when available
    {
        namespace fs = std::filesystem;
        if (orderstate::hasOrder(A)) {
            const std::string inxPath = orderstate::orderName(A);
            if (!inxPath.empty() && fs::exists(inxPath)) {
                std::vector<uint32_t> recnos;
                std::string err;
                if (!scan_load_inx_recnos(inxPath, A.recCount(), recnos, &err)) {
                    std::cout << "Index problem: " << inxPath << " (" << err << "). Using physical order.\n";
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
            // If file is absent, silently fall back to physical order (no scary warning).
        }
    }

    // Fallback: physical order (TOP -> EOF)
    if (!usedIndex) {
        if (A.top() && A.readCurrent()) {
            do {
                if (!while_ok(A)) break;
                run_body_for_current();
            } while (A.skip(1) && A.readCurrent());
        }
    }

    st.active = false; st.lines.clear();
    s_for_triplet.reset(); s_while_expr.reset();
    std::cout << "ENDSCAN: " << matched << " match(es), " << iters << " iteration(s).\n";
}

// -------------- SCAN block capture --------------

void cmd__SCAN_BUFFER(DbArea&, std::istringstream& iss) {
    auto& st = scanblock::state();
    if (!st.active) { std::cout << "No active SCAN.\n"; return; }
    std::string line; std::getline(iss, line);
    line = strip_inline_comment(line);
    if (!line.empty()) st.lines.push_back(line);
}

// Self-register
static bool s_reg = [](){
    dli::registry().add("SCAN",    [](DbArea& a, std::istringstream& s){ cmd_SCAN(a,s); });
    dli::registry().add("ENDSCAN", [](DbArea& a, std::istringstream& s){ cmd_ENDSCAN(a,s); });
    dli::registry().add("__SCAN_BUFFER", [](DbArea& a, std::istringstream& s){ cmd__SCAN_BUFFER(a,s); });
    return true;
}();
