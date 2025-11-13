// src/cli/cmd_wsreport.cpp — WSREPORT [ALL]
// Workspace status with explicit area number + datafile line.
// - WSREPORT           : current area only (prints "(no file open)" if closed)
// - WSREPORT ALL       : lists OPEN areas only (skips empty/closed slots)
//
// Output per open area:
//   Current area: <slot>
//   Datafile: <full-path-or-name>  (<filename>)  Recs: N  Recno: R
//   Order       : PHYSICAL | ASCEND | DESCEND
//   Index file  : (none | <inx path>)
//   Active tag  : (none | <expr-or-stem>)
//
// Depends on public headers:
//   workareas.hpp  : workareas::current_slot(), workareas::count(), workareas::at(i)
//   xbase.hpp      : xbase::DbArea (name(), fields(), recCount(), recno(), isOpen())
//   order_state.hpp: orderstate::{hasOrder,isAscending,orderName}

#include "xbase.hpp"
#include "workareas.hpp"
#include "order_state.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using xbase::DbArea;

// ---------- small string/file utils ----------
static std::string to_upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

static std::string basename_no_ext(const std::string& p) {
    fs::path ph(p);
    return ph.stem().string();
}

static std::string filename_only(const std::string& p) {
    fs::path ph(p);
    return ph.filename().string();
}

static std::string only_printable(std::string_view sv) {
    std::string out; out.reserve(sv.size());
    for (unsigned char c : sv) {
        if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') out.push_back((char)c);
    }
    return out;
}

static bool is_word_boundary(char c) {
    return !std::isalnum((unsigned char)c) && c != '_';
}

static std::optional<std::string> parse_expr_token_after(std::string_view sv, size_t start) {
    while (start < sv.size() && std::isspace((unsigned char)sv[start])) ++start;
    if (start >= sv.size()) return std::nullopt;

    size_t i = start;
    auto is_expr_char = [](unsigned char c){
        return std::isalnum(c) || c == '_' || c == '#';
    };
    while (i < sv.size() && is_expr_char((unsigned char)sv[i])) ++i;

    if (i == start) return std::nullopt;
    return std::string(sv.substr(start, i - start));
}

// Sniff index expression from .inx:
// 1) "expr:" token, 2) whole-word schema field, 3) none.
static std::optional<std::string> sniff_expr_from_inx(const DbArea& A, const std::string& inx_path) {
    std::error_code ec;
    if (!fs::exists(inx_path, ec)) return std::nullopt;

    constexpr size_t MAX_SCAN = 64 * 1024;
    std::ifstream f(inx_path, std::ios::binary);
    if (!f) return std::nullopt;

    std::string buf; buf.resize(MAX_SCAN);
    f.read(&buf[0], (std::streamsize)buf.size());
    size_t n = (size_t)f.gcount();
    buf.resize(n);
    if (buf.empty()) return std::nullopt;

    std::string hay = only_printable(buf);
    if (hay.empty()) return std::nullopt;

    std::string H = to_upper_copy(hay);
    const std::string needle = "EXPR:";
    size_t pos = H.find(needle);
    if (pos != std::string::npos) {
        size_t orig_pos = pos + needle.size();
        if (auto tok = parse_expr_token_after(std::string_view(hay), orig_pos)) {
            return to_upper_copy(*tok);
        }
    }

    std::vector<std::string> fieldsU;
    for (const auto& fd : A.fields()) {
        if (!fd.name.empty()) fieldsU.push_back(to_upper_copy(fd.name));
    }

    for (const auto& u : fieldsU) {
        size_t p = H.find(u);
        while (p != std::string::npos) {
            bool left_ok  = (p == 0) || is_word_boundary(H[p - 1]);
            size_t r = p + u.size();
            bool right_ok = (r >= H.size()) || is_word_boundary(H[r]);
            if (left_ok && right_ok) {
                return u;
            }
            p = H.find(u, p + 1);
        }
    }

    return std::nullopt;
}

// Consider an area "open-ish" only if isOpen() is true AND the name is non-empty.
static bool is_openish(DbArea* A) {
    if (!A) return false;
    if (!A->isOpen()) return false;
    const std::string nm = A->name();
    return !nm.empty();
}

// Print a single open area block.
static void print_open_area(size_t slot, DbArea& A) {
    std::cout << "Current area: " << slot << "\n";

    const std::string full = A.name();
    const std::string file = filename_only(full);
    std::cout << "Datafile: " << full
              << "  (" << file << ")"
              << "  Recs: " << A.recCount()
              << "  Recno: " << A.recno() << "\n";

    bool has = false, asc = true;
    std::string orderName;
    try {
        has = orderstate::hasOrder(A);
        if (has) {
            asc = orderstate::isAscending(A);
            orderName = orderstate::orderName(A);
        }
    } catch (...) { has = false; }

    std::cout << "  Order       : " << (has ? (asc ? "ASCEND" : "DESCEND") : "PHYSICAL") << "\n";

    if (has && !orderName.empty()) {
        std::cout << "  Index file  : " << orderName << "\n";
        if (auto expr = sniff_expr_from_inx(A, orderName)) {
            std::cout << "  Active tag  : " << *expr << "\n";
        } else {
            std::cout << "  Active tag  : " << basename_no_ext(orderName) << "\n";
        }
    } else {
        std::cout << "  Index file  : (none)\n";
        std::cout << "  Active tag  : (none)\n";
    }
}

// Print a closed/empty area block (used only for single-area WSREPORT).
static void print_closed_area(size_t slot) {
    std::cout << "Current area: " << slot << "\n";
    std::cout << "Datafile: (no file open)\n";
    std::cout << "  Order       : PHYSICAL\n";
    std::cout << "  Index file  : (none)\n";
    std::cout << "  Active tag  : (none)\n";
}

void cmd_WSREPORT(DbArea&, std::istringstream& args) {
    std::string tok;
    if (!(args >> tok)) {
        // Current area only (print even if closed, but clearly)
        size_t cur = workareas::current_slot();
        DbArea* A = workareas::at(cur);
        std::cout << "DotTalk Status Report\n\n";
        if (!is_openish(A)) {
            print_closed_area(cur);
            return;
        }
        print_open_area(cur, *A);
        return;
    }

    std::string tokU = to_upper_copy(tok);
    if (tokU == "ALL") {
        // List open areas only; skip closed/empty slots to avoid clutter.
        std::cout << "DotTalk Status Report (ALL)\n\n";
        size_t n = workareas::count();
        bool any = false;
        for (size_t i = 0; i < n; ++i) {
            DbArea* A = workareas::at(i);
            if (!is_openish(A)) continue;
            if (any) std::cout << "\n";
            print_open_area(i, *A);
            any = true;
        }
        if (!any) std::cout << "No open areas.\n";
        return;
    }

    // Unknown switch -> treat like current area.
    size_t cur = workareas::current_slot();
    DbArea* A = workareas::at(cur);
    std::cout << "DotTalk Status Report\n\n";
    if (!is_openish(A)) {
        print_closed_area(cur);
        return;
    }
    print_open_area(cur, *A);
}
