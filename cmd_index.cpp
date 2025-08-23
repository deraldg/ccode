// src/cli/cmd_index.cpp
#include "xbase.hpp"
#include "textio.hpp"
#include "xindex/simple_index.hpp"
#include "order_state.hpp"
#include "order_hooks.hpp"

// NEW: wire in-memory manager so STATUS can show expressions
#include "xindex/attach.hpp"
#include "xindex/index_spec.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

namespace fs = std::filesystem;
using xbase::DbArea;
using xindex::IndexMeta;
using xindex::SimpleIndex;

static std::string up(std::string s)            { return textio::up(s); }
static std::string trim(const std::string& s)   { return textio::trim(s); }
static bool ieq(const std::string& a, const std::string& b){ return up(a)==up(b); }

static bool read_quoted_or_token(std::istringstream& iss, std::string& out) {
    iss >> std::ws;
    if (!iss.good()) return false;
    int c = iss.peek();
    if (c=='"' || c=='\'') {
        char q = (char)iss.get();
        std::ostringstream tmp;
        for (int ch; (ch=iss.get())!=EOF;) {
            if (ch==q) break;
            tmp << (char)ch;
        }
        out = tmp.str();
        return true;
    }
    return (bool)(iss >> out);
}

// TRIM-AWARE field lookup (fix for trailing spaces in DBF header names)
static int findFieldIndex(const DbArea& A, const std::string& name) {
    const auto& F = A.fields();
    std::string U = up(trim(name));
    for (size_t j = 0; j < F.size(); ++j) {
        if (up(trim(F[j].name)) == U)   // trim the DBF header name too
            return (int)j;
    }
    return -1;
}

static bool isMemo(const DbArea& A, int idx) {
    const auto& F = A.fields();
    if (idx < 0 || (size_t)idx >= F.size()) return false;
    int T = std::toupper((unsigned char)F[idx].type);
    return T == 'M';
}

// TRIM when listing available fields (nice UX)
static void listAvailableFields(const DbArea& A) {
    const auto& F = A.fields();
    for (size_t j=0; j<F.size(); ++j) {
        if (j) std::cout << ", ";
        std::cout << trim(F[j].name);
    }
    std::cout << "\n";
}

static void collectFieldIndicesFromExpr(const DbArea& A,
                                        const std::string& expr,
                                        std::vector<int>& out)
{
    // Very simple splitter on '+', ignoring quoted literals
    std::string tok;
    bool inQ = false; char q = 0;
    auto pushTok = [&](){
        std::string t = trim(tok);
        if (!t.empty()) {
            bool quoted = (t.size()>=2) && ((t.front()=='"'&&t.back()=='"')||(t.front()=='\''&&t.back()=='\''));
            if (!quoted) {
                int idx = findFieldIndex(A, t);
                if (idx >= 0 && !isMemo(A, idx)) {
                    if (std::find(out.begin(), out.end(), idx) == out.end())
                        out.push_back(idx);
                }
            }
        }
        tok.clear();
    };

    for (char c : expr) {
        if (inQ) {
            tok.push_back(c);
            if (c == q) inQ = false;
        } else {
            if (c=='"' || c=='\'') { inQ = true; q = c; tok.push_back(c); }
            else if (c=='+') pushTok();
            else tok.push_back(c);
        }
    }
    pushTok();
}

void cmd_INDEX(DbArea& A, std::istringstream& iss) {
    if (!A.isOpen()) { std::cout << "No table open.\n"; return; }

    std::string onTok;
    if (!(iss >> onTok) || !ieq(onTok, "ON")) {
        std::cout << "Usage:\n"
                     "  INDEX ON <field> [ASC|DESC] [TO <name>]\n"
                     "  INDEX ON EXPR \"<expr>\" [ASC|DESC] [TO <name>]\n";
        return;
    }

    std::string first;
    if (!read_quoted_or_token(iss, first)) {
        std::cout << "INDEX: missing <field> or EXPR.\n";
        return;
    }

    IndexMeta meta;
    std::string toName;
    bool hasExpr = false;

    if (ieq(first, "EXPR")) {
        hasExpr = true;
        if (!read_quoted_or_token(iss, meta.expression) || meta.expression.empty()) {
            std::cout << "INDEX ON EXPR: missing expression string.\n";
            return;
        }
    } else {
        meta.expression = first; // field name
    }

   // Optional tail: [ASC|DESC] [TO <name>] | [TAG <name>] 
    std::string tok;
    while (iss >> tok) {
        if (ieq(tok, "ASC")) meta.ascending = true;
        else if (ieq(tok, "DESC")) meta.ascending = false;
        else if (ieq(tok, "TO") || ieq(tok, "TAG")) {
            if (!read_quoted_or_token(iss, toName)) {
                std::cout << "INDEX: TO requires a file name.\n";
                return;
            }
        }
    }

    // Determine referenced fields (and block MEMO)
    if (!hasExpr) {
        int idx = findFieldIndex(A, meta.expression);
        if (idx < 0) {
            std::cout << "INDEX: unknown field '" << meta.expression << "'.\nAvailable: ";
            listAvailableFields(A);
            std::cout << "Tip: INDEX ON EXPR \"LAST_NAME + FIRST_NAME\"\n";
            return;
        }
        if (isMemo(A, idx)) {
            std::cout << "Cannot index MEMO field: " << meta.expression << "\n";
            return;
        }
        meta.field_indices.push_back(idx);
    } else {
        collectFieldIndicesFromExpr(A, meta.expression, meta.field_indices);
        if (meta.field_indices.empty()) {
            std::cout << "INDEX ON EXPR: expression doesn't reference any table fields; "
                         "this would create a constant index. Aborting.\n";
            return;
        }
    }

    // Output path: default next to the DBF as <dbfname>.inx
    fs::path out;
    {
        fs::path dbp = A.name();                 // may be just "students.dbf"
        fs::path dbdir = dbp.parent_path();      // may be empty
        if (toName.empty()) {
            out = dbp; out.replace_extension(".inx");
        } else {
            fs::path t = toName;
            if (t.extension().empty()) t.replace_extension(".inx");
            out = t.parent_path().empty() ? (dbdir / t.filename()) : t;
        }
    }

    std::string err;
    try {
        if (!SimpleIndex::build_and_save(A, meta, out, &err)) {
            std::cout << "INDEX failed: " << err << "\n";
            return;
        }
    } catch (const std::exception& ex) {
        std::cout << "INDEX failed: " << ex.what() << "\n";
        return;
    } catch (...) {
        std::cout << "INDEX failed: unknown error.\n";
        return;
    }

    std::cout << "Index written: " << out.filename().string()
              << "  (expr: " << meta.expression << ", "
              << (meta.ascending ? "ASC" : "DESC") << ")\n";

    // Make it the active order for this area
    orderstate::setOrder(A, out.string());

    // ==================== NEW: sync in-memory IndexManager ====================
    // So STATUS can display:   * TAG  -> FIELD[+FIELD2]
    try {
        xindex::IndexSpec spec;
        spec.tag       = up(out.stem().string());
        spec.ascending = meta.ascending;
        spec.unique    = false;
        spec.fields.clear();
        const auto& F = A.fields();
        for (int fi : meta.field_indices) {
            if (fi >= 0 && (size_t)fi < F.size()) {
                spec.fields.push_back(up(trim(F[fi].name)));
            }
        }
        if (spec.fields.empty()) {
            // Fallback: single token from expression (uppercased)
            spec.fields.push_back(up(trim(meta.expression)));
        }
        auto& mgr = xindex::ensure_manager(A);
        mgr.ensure_tag(spec);
        mgr.set_active(spec.tag);
    } catch (...) {
        // If attach layer not available, silently continue.
    }
}
