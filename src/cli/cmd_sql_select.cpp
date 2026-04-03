// src/cli/cmd_sql.cpp
#include "xbase.hpp"
#include "xbase_field_getters.hpp"
#include "record_view.hpp"
#include "textio.hpp"
// #include "predicate_eval.hpp" // reserved

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "cli/expr/api.hpp"
#include "cli/expr/for_parser.hpp"

// ---------- helpers: trim + uppercase ----------
static inline std::string dt_trim(std::string s) {
    auto sp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static inline std::string dt_upcase(std::string s) {
    for (auto &ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}
static inline bool ieq(std::string a, std::string b) {
    return dt_upcase(dt_trim(std::move(a))) == dt_upcase(dt_trim(std::move(b)));
}

// Map getters for DotTalk glue (case-insensitive string compares)
#define DOTTALK_GET_FIELD_STR(area, name)  dt_upcase(dt_trim(xfg::getFieldAsString(area, name)))
#define DOTTALK_GET_FIELD_NUM(area, name)  xfg::getFieldAsNumber(area, name)
#include "cli/expr/glue_xbase.hpp"

// SQL normalizer
#include "expr/sql_normalize.hpp"

// External ? provided by DotTalk expr
dottalk::expr::CompileResult compile_where(const std::string& text);

namespace {

static inline std::string up(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

enum class DelMode { SkipDeleted, OnlyDeleted, IncludeAll };

struct Opts {
    DelMode     mode    = DelMode::SkipDeleted;
    bool        haveFor = false;
    std::string forRaw;    // user WHERE after "FOR" (or the whole tail if no FOR)
    std::string tailRaw;   // everything after SQL keyword (for debug echo)
};

// Parse: SQL [COUNT] [ALL|DELETED] [FOR <expr> | <expr>]
static Opts parse_opts(std::istringstream& iss) {
    Opts o;

    // Rebuild tail from the stream
    std::string rest;
    {
        const std::string& all = iss.str();
        auto pos = iss.tellg();
        if (pos != std::istringstream::pos_type(-1)) {
            size_t i = static_cast<size_t>(pos);
            if (i < all.size()) rest = all.substr(i);
        } else rest = all;
    }
    o.tailRaw = dt_trim(rest);

    std::istringstream head(o.tailRaw);

    // Optional COUNT
    std::streampos afterFirst = head.tellg();
    std::string t;
    if (head >> t) {
        if (up(t) != "COUNT") {
            head.clear();
            head.seekg(afterFirst);
        }
    }

    // Optional ALL | DELETED
    std::streampos afterMode = head.tellg();
    std::string modeTok;
    if (head >> modeTok) {
        auto M = up(modeTok);
        if      (M == "ALL")     o.mode = DelMode::IncludeAll;
        else if (M == "DELETED") o.mode = DelMode::OnlyDeleted;
        else { head.clear(); head.seekg(afterMode); }
    }

    // Remaining string
    std::string tail; std::getline(head, tail);
    tail = dt_trim(tail);

    // Accept both "FOR <expr>" and plain "<expr>"
    if (!tail.empty()) {
        auto U = up(tail);
        if (U.rfind("FOR", 0) == 0) {
            o.haveFor = true;
            o.forRaw = dt_trim(tail.substr(3));
        } else {
            o.haveFor = true;
            o.forRaw = tail;
        }
    }

    return o;
}

// Extract candidate field names from a normalized DotTalk expr.
// Skips AND/OR/NOT, parens, operators, numerics.
static std::vector<std::string> extract_field_names(const std::string& norm) {
    static const std::unordered_set<std::string> stop_words{ "AND","OR","NOT" };

    std::vector<std::string> fields;
    std::unordered_set<std::string> seen;

    auto is_word = [](char c)->bool {
        return (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_';
    };

    for (size_t i = 0; i < norm.size();) {
        char c = norm[i];

        // Skip quoted literals
        if (c == '"' || c == '\'') {
            char q = c; ++i;
            while (i < norm.size()) {
                char d = norm[i++];
                if (d == q) {
                    if (i < norm.size() && norm[i] == q) { ++i; continue; }
                    break;
                }
            }
            continue;
        }

        if (is_word(c)) {
            size_t j = i+1;
            while (j < norm.size() && is_word(norm[j])) ++j;
            std::string tok = norm.substr(i, j-i);

            bool numeric = !tok.empty() &&
                           std::all_of(tok.begin(), tok.end(), [](char ch){ return ch>='0' && ch<='9'; });

            if (!numeric) {
                if (!stop_words.count(tok) && !seen.count(tok)) {
                    seen.insert(tok);
                    fields.push_back(tok);
                }
            }
            i = j;
            continue;
        }

        ++i;
    }
    return fields;
}

// ---------- Simple evaluator (FIELD <op> VALUE) [+ AND/OR/NOT] ----------

// A tiny token stream for normalized DotTalk-ish input.
enum class STok {
    Ident, String, Number,
    Eq, Ne, Gt, Lt, Ge, Le,
    And, Or, Not,
    LParen, RParen,
    End
};
struct SToken { STok k; std::string text; };

static bool is_digit(char c){ return c>='0'&&c<='9'; }
static bool is_word_char(char c){ return (c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'; }

static std::vector<SToken>stok(const std::string& s){
    std::vector<SToken> out; size_t i=0,n=s.size();
    auto push=[&](STok k,std::string v={}){ out.push_back({k,std::move(v)}); };
    while(i<n){
        char c=s[i];
        if(c==' '||c=='\t'||c=='\r'||c=='\n'){++i;continue;}
        if(c=='"'||c=='\''){
            char q=c; std::string buf; buf.push_back(q); ++i;
            while(i<n){
                char d=s[i++]; buf.push_back(d);
                if(d==q){ if(i<n && s[i]==q){ buf.push_back(s[i++]); continue; } break; }
            }
            push(STok::String,buf); continue;
        }
        if(is_digit(c)){
            size_t j=i+1; bool dot=false;
            while(j<n){ char d=s[j]; if(is_digit(d)){++j;continue;} if(d=='.'&&!dot){dot=true;++j;continue;} break; }
            push(STok::Number,s.substr(i,j-i)); i=j; continue;
        }
        if(is_word_char(c)){
            size_t j=i+1; while(j<n && is_word_char(s[j])) ++j;
            std::string w=s.substr(i,j-i);
            std::string U=up(w);
            if(U==".AND."||U=="AND"){ push(STok::And); i=j; continue; }
            if(U==".OR." ||U=="OR"){  push(STok::Or);  i=j; continue; }
            if(U==".NOT."||U=="NOT"){ push(STok::Not); i=j; continue; }
            push(STok::Ident, U); i=j; continue;
        }
        if(i+1<n){
            char d=s[i+1];
            if(c=='<'&&d=='>'){ push(STok::Ne); i+=2; continue; }
            if(c=='<'&&d=='='){ push(STok::Le); i+=2; continue; }
            if(c=='>'&&d=='='){ push(STok::Ge); i+=2; continue; }
        }
        if(c=='='){ push(STok::Eq); ++i; continue; }
        if(c=='<'){ push(STok::Lt); ++i; continue; }
        if(c=='>'){ push(STok::Gt); ++i; continue; }
        if(c=='('){ push(STok::LParen); ++i; continue; }
        if(c==')'){ push(STok::RParen); ++i; continue; }
        ++i; // skip unknown
    }
    push(STok::End);
    return out;
}

struct Clause { std::string field; STok op; std::string sval; double nval=0; bool isNum=false; };

static bool parse_simple_chain(const std::vector<SToken>& tks,
                               std::vector<Clause>& outClauses,
                               std::vector<STok>& outBools)
{
    // Grammar (restricted):
    //   EXPR := TERM { (AND|OR) TERM }*
    //   TERM := [NOT] ( (IDENT OP VALUE) | '(' EXPR ')' )
    size_t i=0;
    auto peek=[&](size_t k=0)->const SToken&{ return tks[i+k]; };
    auto eat=[&](){ return tks[i++]; };

    std::function<bool()> parse_expr, parse_term, parse_paren;

    auto parse_value = [&](std::string& sval, double& nval, bool& isNum)->bool{
        const auto& tk = peek();
        if(tk.k==STok::String){
            // strip quotes but keep doubled inner quotes intact
            std::string v=tk.text;
            if(!v.empty()&&(v.front()=='"'||v.front()=='\'')) v.erase(v.begin());
            if(!v.empty()&&(v.back()=='"' ||v.back()=='\'' )) v.pop_back();
            sval = dt_upcase(dt_trim(v));
            isNum=false; eat(); return true;
        }
        if(tk.k==STok::Number){
            sval = tk.text; try{ nval = std::stod(sval); isNum=true; } catch(...){ isNum=false; }
            eat(); return true;
        }
        if(tk.k==STok::Ident){
            // treat bare word as string value (uppercased)
            sval = tk.text; isNum=false; eat(); return true;
        }
        return false;
    };

    auto parse_clause = [&]()->bool{
        bool neg=false;
        if(peek().k==STok::Not){ neg=true; eat(); }
        if(peek().k==STok::LParen){
            // We don't build nested Clause trees; declare it's not simple.
            return false;
        }
        if(peek().k!=STok::Ident) return false;
        Clause c{};
        c.field = peek().text; eat();
        STok op = peek().k;
        if(op!=STok::Eq && op!=STok::Ne && op!=STok::Gt && op!=STok::Lt && op!=STok::Ge && op!=STok::Le) return false;
        c.op = op; eat();
        if(!parse_value(c.sval,c.nval,c.isNum)) return false;
        if(neg){
            // flip operator for NOT IDENT <op> VALUE
            if(c.op==STok::Eq) c.op=STok::Ne;
            else if(c.op==STok::Ne) c.op=STok::Eq;
            else if(c.op==STok::Gt) c.op=STok::Le;
            else if(c.op==STok::Ge) c.op=STok::Lt;
            else if(c.op==STok::Lt) c.op=STok::Ge;
            else if(c.op==STok::Le) c.op=STok::Gt;
        }
        outClauses.push_back(std::move(c));
        return true;
    };

    if(!parse_clause()) return false;

    while(peek().k==STok::And || peek().k==STok::Or){
        outBools.push_back(peek().k); eat();
        if(!parse_clause()) return false;
    }
    return peek().k==STok::End;
}

static bool eval_clause(const Clause& c, xbase::DbArea& A){
    // Fetch both string (upper-trim) and numeric
    std::string fs; double fn=0; bool fnum_ok=false;
    try { fs = dt_upcase(dt_trim(xfg::getFieldAsString(A, c.field))); } catch(...) { fs=""; }
    try { fn = xfg::getFieldAsNumber(A, c.field); fnum_ok = std::isfinite(fn); } catch(...) { fnum_ok=false; }

    auto cmp_str = [&](const std::string& L, const std::string& R)->int{
        if(L==R) return 0;
        return (L<R)?-1:+1;
    };

    if(c.isNum && fnum_ok){
        double L=fn, R=c.nval;
        switch(c.op){
            case STok::Eq: return L==R;
            case STok::Ne: return L!=R;
            case STok::Gt: return L> R;
            case STok::Ge: return L>=R;
            case STok::Lt: return L< R;
            case STok::Le: return L<=R;
            default: return false;
        }
    }

    // string compare (case-insensitive already)
    int rel = cmp_str(fs, c.isNum ? std::to_string(c.nval) : c.sval);
    switch(c.op){
        case STok::Eq: return rel==0;
        case STok::Ne: return rel!=0;
        case STok::Gt: return rel>0;
        case STok::Ge: return rel>=0;
        case STok::Lt: return rel<0;
        case STok::Le: return rel<=0;
        default: return false;
    }
}

static bool eval_chain(const std::vector<Clause>& cs, const std::vector<STok>& ops, xbase::DbArea& A){
    bool acc = eval_clause(cs[0], A);
    for(size_t i=0;i<ops.size();++i){
        bool rhs = eval_clause(cs[i+1], A);
        if(ops[i]==STok::And) acc = acc && rhs;
        else                  acc = acc || rhs;
    }
    return acc;
}

} // anon

void cmd_SQL_SELECT(xbase::DbArea& A, std::istringstream& iss) {
    if (!A.isOpen()) { std::cout << "No file open\n"; return; }

    const Opts opt = parse_opts(iss);

    auto include_row = [&](bool deleted)->bool {
        if (opt.mode == DelMode::SkipDeleted && deleted) return false;
        if (opt.mode == DelMode::OnlyDeleted && !deleted) return false;
        return true;
    };

    std::unique_ptr<dottalk::expr::Expr> prog;
    std::string normalized;
    std::vector<std::string> debug_fields;

    // --- DEBUG HEADER ---
    std::cout << "SQL DEBUG ? raw: \"" << opt.tailRaw << "\"\n";

    // Try to prepare either simple-chain evaluator or DotTalk program
    bool use_simple=false;
    std::vector<Clause> simpleClauses;
    std::vector<STok>   simpleOps;

    if (opt.haveFor) {
        normalized = sqlnorm::sql_to_dottalk_where(opt.forRaw);
        std::cout << "SQL DEBUG ? normalized: " << normalized << "\n";

        // Gather fields for per-record debug printing
        debug_fields = extract_field_names(normalized);
        if (!debug_fields.empty()) {
            std::cout << "SQL DEBUG ? fields: ";
            for (size_t i=0;i<debug_fields.size();++i) {
                if (i) std::cout << ", ";
                std::cout << debug_fields[i];
            }
            std::cout << "\n";
        } else {
            std::cout << "SQL DEBUG ? fields: (none detected)\n";
        }

        // 1) Try simple-chain path first
        auto tks = stok(normalized);
        if (parse_simple_chain(tks, simpleClauses, simpleOps)) {
            use_simple = true;
        } else {
            // 2) Fall back to DotTalk parser
            auto cr = compile_where(normalized);
            if (!cr) {
                std::cout << "Syntax error in FOR: " << cr.error << "\n";
                return;
            }
            prog = std::move(cr.program);
        }
    } else {
        std::cout << "SQL DEBUG ? no clause (plain COUNT)\n";
    }

    long long cnt = 0;
    long long scanned = 0;

    if (A.top() && A.readCurrent()) {
        do {
            ++scanned;

            if (!include_row(A.isDeleted())) continue;

            bool ok=false;
            if (!opt.haveFor) {
                ok = true;
            } else if (use_simple) {
                ok = eval_chain(simpleClauses, simpleOps, A);
            } else {
                auto rv = dottalk::expr::glue::make_record_view(A);
                ok = prog->eval(rv);
            }

            // Per-record debug print
            if (debug_fields.empty()) {
                std::cout << "[rec " << A.recno() << "] => " << (ok ? "true" : "false") << "\n";
            } else {
                std::ostringstream fv;
                fv << "[rec " << A.recno() << "] ";
                for (size_t i=0;i<debug_fields.size();++i) {
                    const std::string& fld = debug_fields[i];

                    std::string s;
                    try { s = xfg::getFieldAsString(A, fld); } catch (...) { s = "(ERR)"; }
                    fv << fld << "=\"" << dt_upcase(dt_trim(s)) << "\"";

                    try {
                        double n = xfg::getFieldAsNumber(A, fld);
                        if (std::isfinite(n)) fv << " (num=" << n << ")";
                    } catch (...) {}

                    if (i+1 < debug_fields.size()) fv << ", ";
                }
                fv << " => " << (ok ? "true" : "false");
                std::cout << fv.str() << "\n";
            }

            if (ok) ++cnt;

        } while (A.skip(+1) && A.readCurrent());
    }

    std::cout << "SQL DEBUG ? scanned: " << scanned << "  matched: " << cnt << "\n";
    std::cout << cnt << "\n";
}




