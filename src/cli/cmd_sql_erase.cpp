#include "xbase.hpp"
#include "xbase_field_getters.hpp"
#include "textio.hpp"
#include "expr/sql_normalize.hpp"
#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sqlmini {
enum class TokK { Ident, String, Number, Eq, Ne, Gt, Lt, Ge, Le, And, Or, Not, LParen, RParen, End };
struct Tok { TokK k; std::string text; };
static inline std::string up(std::string s){ for(char& c: s) c=(char)std::toupper((unsigned char)c); return s; }
static inline bool isdigitc(char c){ return c>='0' && c<='9'; }
static std::vector<Tok> lex(const std::string& s){
    std::vector<Tok> out; size_t i=0,n=s.size();
    auto push=[&](TokK k, std::string v={}){ out.push_back({k,std::move(v)}); };
    while(i<n){
        char c=s[i];
        if(c==' '||c=='\t'||c=='\r'||c=='\n'){ ++i; continue; }
        if(c=='\'' || c=='"'){
            char q=c; std::string buf; ++i;
            while(i<n){
                char d=s[i++];
                if(d==q){ if(i<n && s[i]==q){ buf.push_back(q); ++i; continue; } break; }
                buf.push_back(d);
            }
            push(TokK::String, buf); continue;
        }
        if(isdigitc(c)){
            size_t j=i+1; bool dot=false;
            while(j<n){ char d=s[j]; if(isdigitc(d)){++j;continue;} if(d=='.'&&!dot){dot=true;++j;continue;} break; }
            push(TokK::Number, s.substr(i,j-i)); i=j; continue;
        }
        if(std::isalpha((unsigned char)c) || c=='_'){
            size_t j=i+1; while(j<n && (std::isalnum((unsigned char)s[j])||s[j]=='_')) ++j;
            std::string w = up(s.substr(i,j-i));
            if(w==".AND."||w=="AND"){ push(TokK::And); i=j; continue; }
            if(w==".OR." ||w=="OR"){  push(TokK::Or);  i=j; continue; }
            if(w==".NOT."||w=="NOT"){ push(TokK::Not); i=j; continue; }
            push(TokK::Ident, w); i=j; continue;
        }
        if(i+1<n){
            char d=s[i+1];
            if(c=='<'&&d=='>'){ push(TokK::Ne); i+=2; continue; }
            if(c=='<'&&d=='='){ push(TokK::Le); i+=2; continue; }
            if(c=='>'&&d=='='){ push(TokK::Ge); i+=2; continue; }
        }
        if(c=='='){ push(TokK::Eq); ++i; continue; }
        if(c=='<'){ push(TokK::Lt); ++i; continue; }
        if(c=='>'){ push(TokK::Gt); ++i; continue; }
        if(c=='('){ push(TokK::LParen); ++i; continue; }
        if(c==')'){ push(TokK::RParen); ++i; continue; }
        ++i;
    }
    push(TokK::End);
    return out;
}
struct Clause { std::string field; TokK op; std::string sval; double nval=0; bool isNum=false; };
static bool parse_simple(const std::vector<Tok>& tks, std::vector<Clause>& out, std::vector<TokK>& ops){
    size_t i=0;
    auto peek=[&](size_t k=0)->const Tok&{ return tks[i+k]; };
    auto eat=[&](){ return tks[i++]; };
    auto parse_value=[&](std::string& sval,double& nval,bool& isNum)->bool{
        const auto& tk=peek();
        if(tk.k==TokK::String){ sval=tk.text; isNum=false; eat(); return true; }
        if(tk.k==TokK::Number){ sval=tk.text; nval=std::stod(sval); isNum=true; eat(); return true; }
        if(tk.k==TokK::Ident){  sval=tk.text; isNum=false; eat(); return true; }
        return false;
    };
    auto parse_clause=[&]()->bool{
        bool neg=false; if(peek().k==TokK::Not){ neg=true; eat(); }
        if(peek().k!=TokK::Ident) return false;
        Clause c{}; c.field = peek().text; eat();
        TokK op = peek().k;
        if(op!=TokK::Eq && op!=TokK::Ne && op!=TokK::Gt && op!=TokK::Lt && op!=TokK::Ge && op!=TokK::Le) return false;
        c.op = op; eat();
        if(!parse_value(c.sval,c.nval,c.isNum)) return false;
        if(neg){
            if(c.op==TokK::Eq) c.op=TokK::Ne;
            else if(c.op==TokK::Ne) c.op=TokK::Eq;
            else if(c.op==TokK::Gt) c.op=TokK::Le;
            else if(c.op==TokK::Ge) c.op=TokK::Lt;
            else if(c.op==TokK::Lt) c.op=TokK::Ge;
            else if(c.op==TokK::Le) c.op=TokK::Gt;
        }
        out.push_back(std::move(c));
        return true;
    };
    if(!parse_clause()) return false;
    while(peek().k==TokK::And || peek().k==TokK::Or){
        ops.push_back(peek().k); eat();
        if(!parse_clause()) return false;
    }
    return peek().k==TokK::End;
}
static bool cmp_clause(const Clause& c, xbase::DbArea& A){
    std::string fs; double fn=0; bool fnum_ok=false;
    try { fs = xfg::getFieldAsString(A, c.field); } catch(...) { fs=""; }
    try { fn = xfg::getFieldAsNumber(A, c.field); fnum_ok = std::isfinite(fn); } catch(...) { fnum_ok=false; }
    auto uptrim=[](std::string s){ while(!s.empty() && s.back()==' ') s.pop_back(); for(char& d: s) d=(char)std::toupper((unsigned char)d); return s; };
    std::string Ls = uptrim(fs);
    std::string Rs = uptrim(c.isNum ? std::to_string(c.nval) : c.sval);
    if(c.isNum && fnum_ok){
        double L = fn, R = c.nval;
        switch(c.op){ case TokK::Eq: return L==R; case TokK::Ne: return L!=R; case TokK::Gt: return L>R; case TokK::Ge: return L>=R; case TokK::Lt: return L<R; case TokK::Le: return L<=R; default: return false; }
    }
    int rel = 0; if(Ls<Rs) rel=-1; else if(Ls>Rs) rel=+1;
    switch(c.op){ case TokK::Eq: return rel==0; case TokK::Ne: return rel!=0; case TokK::Gt: return rel>0; case TokK::Ge: return rel>=0; case TokK::Lt: return rel<0; case TokK::Le: return rel<=0; default: return false; }
}
static bool eval_chain(const std::vector<Clause>& cs, const std::vector<TokK>& ops, xbase::DbArea& A){
    bool acc = cmp_clause(cs[0], A);
    for(size_t i=0;i<ops.size();++i){
        bool rhs = cmp_clause(cs[i+1], A);
        if(ops[i]==TokK::And) acc = acc && rhs;
        else                  acc = acc || rhs;
    }
    return acc;
}
static bool where_match(xbase::DbArea& A, const std::string& whereRaw){
    if(whereRaw.empty()) return true;
    auto norm = sqlnorm::sql_to_dottalk_where(whereRaw);
    auto tks = lex(norm);
    std::vector<Clause> cs; std::vector<TokK> ops;
    if(!parse_simple(tks, cs, ops)) return false;
    return eval_chain(cs, ops, A);
}
} // namespace sqlmini

static inline std::string dt_trim(std::string s){
    auto sp=[](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while(!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while(!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static inline std::string up(std::string s){ for(char& c: s) c=(char)std::toupper((unsigned char)c); return s; }

void cmd_SQL_ERASE(xbase::DbArea& A, std::istringstream& iss){
    if(!A.isOpen()){ std::cout<<"No file open\n"; return; }

    std::string rest; std::getline(iss, rest); rest = dt_trim(rest);
    if(up(rest).rfind("FROM ", 0)!=0){ std::cout<<"ERASE syntax: ERASE FROM <table> WHERE <expr>\n"; return; }
    std::string tail = dt_trim(rest.substr(5));

    std::string table = tail;
    std::string wherePart;
    auto U = up(tail);
    auto wpos = U.find(" WHERE ");
    if(wpos!=std::string::npos){
        table = dt_trim(tail.substr(0, wpos));
        wherePart = dt_trim(tail.substr(wpos+7));
    }

    if(wherePart.empty()){ std::cout<<"ERASE requires WHERE\n"; return; }

    long long touched=0, scanned=0;
    if(A.top() && A.readCurrent()){
        do{
            ++scanned;
            if(A.isDeleted()) continue;
            if(!sqlmini::where_match(A, wherePart)) continue;
            A.deleteCurrent();
            ++touched;
        }while(A.skip(+1) && A.readCurrent());
    }
    std::cout<<touched<<" row(s) erased (scanned "<<scanned<<")\n";
}



