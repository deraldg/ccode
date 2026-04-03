// ===============================================
// include/where_eval_shared.hpp
// Thread-safe shared WHERE/SQL evaluator + LRU cache (env-tunable)
// ===============================================
#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <memory>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <cstdlib>     // std::getenv
#include <mutex>
#include <optional>
#include <sstream>
#include <atomic>

#include "xbase.hpp"
#include "xbase_field_getters.hpp"
#include "cli/expr/api.hpp"
#include "cli/expr/for_parser.hpp"
#include "cli/expr/glue_xbase.hpp"
#include "cli/expr/sql_normalize.hpp"
#include <cmath>


namespace where_eval {

// ---------- trim / upcase ----------
inline std::string dt_trim(std::string s) {
    auto sp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
inline std::string dt_upcase(std::string s) {
    for (auto &ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

// ---------- Field name extraction (for DEBUG prints) ----------
inline std::vector<std::string> extract_field_names(const std::string& norm) {
    static const std::unordered_set<std::string> stop_words{ "AND","OR","NOT" };
    std::vector<std::string> fields;
    std::unordered_set<std::string> seen;

    auto is_word = [](char c)->bool {
        return (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_';
    };

    for (size_t i = 0; i < norm.size();) {
        char c = norm[i];

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
                std::string U = dt_upcase(dt_trim(tok));
                if (!stop_words.count(U) && !seen.count(U)) {
                    seen.insert(U);
                    fields.push_back(U);
                }
            }
            i = j; continue;
        }
        ++i;
    }
    return fields;
}

// ---------- Simple evaluator (tokenizer + chain) ----------
enum class STok {
    Ident, String, Number,
    Eq, Ne, Gt, Lt, Ge, Le,
    And, Or, Not,
    LParen, RParen,
    End
};
struct SToken { STok k; std::string text; };

inline bool is_digit(char c){ return c>='0'&&c<='9'; }
inline bool is_word_char(char c){ return (c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'; }

inline std::vector<SToken> stok(const std::string& s){
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
            std::string U=dt_upcase(dt_trim(w));
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
        ++i;
    }
    push(STok::End);
    return out;
}

struct Clause { std::string field; STok op; std::string sval; double nval=0; bool isNum=false; };

inline bool parse_simple_chain(const std::vector<SToken>& tks,
                               std::vector<Clause>& outClauses,
                               std::vector<STok>& outBools)
{
    size_t i=0;
    auto peek=[&](size_t k=0)->const SToken&{ return tks[i+k]; };
    auto eat=[&](){ return tks[i++]; };

    auto parse_value = [&](std::string& sval, double& nval, bool& isNum)->bool{
        const auto& tk = peek();
        if(tk.k==STok::String){
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
            sval = tk.text; isNum=false; eat(); return true;
        }
        return false;
    };

    auto parse_clause = [&]()->bool{
        bool neg=false;
        if(peek().k==STok::Not){ neg=true; eat(); }
        if(peek().k==STok::LParen){
            return false; // indicates "non-simple"
        }
        if(peek().k!=STok::Ident) return false;
        Clause c{};
        c.field = peek().text; eat();
        STok op = peek().k;
        if(op!=STok::Eq && op!=STok::Ne && op!=STok::Gt && op!=STok::Lt && op!=STok::Ge && op!=STok::Le) return false;
        c.op = op; eat();
        if(!parse_value(c.sval,c.nval,c.isNum)) return false;
        if(neg){
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

inline bool eval_clause(const Clause& c, xbase::DbArea& A){
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

    int rel = cmp_str(fs, c.isNum ? dt_upcase(std::to_string(c.nval)) : c.sval);
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

inline bool eval_chain(const std::vector<Clause>& cs, const std::vector<STok>& ops, xbase::DbArea& A){
    bool acc = eval_clause(cs[0], A);
    for(size_t i=0;i<ops.size();++i){
        bool rhs = eval_clause(cs[i+1], A);
        if(ops[i]==STok::And) acc = acc && rhs;
        else                  acc = acc || rhs;
    }
    return acc;
}

// ---------- Compiled program + Thread-safe LRU cache ----------
struct EvalProgram {
    bool simple=false;
    std::vector<Clause> clauses;
    std::vector<STok>   ops;
    std::unique_ptr<dottalk::expr::Expr> prog;
};
struct CacheEntry {
    std::shared_ptr<EvalProgram> plan;
    std::vector<std::string>     fields; // extracted for DEBUG
};

constexpr size_t DEFAULT_CACHE_CAPACITY = 256;

inline size_t clamp_capacity(size_t v) {
    if (v < 16) return 16;
    if (v > 65536) return 65536;
    return v;
}

inline size_t env_capacity() {
    static std::once_flag once;
    static size_t cap = DEFAULT_CACHE_CAPACITY;
    std::call_once(once, []{
        const char* s = std::getenv("DOTTALK_WHERECACHE");
        if (!s || !*s) { cap = DEFAULT_CACHE_CAPACITY; return; }
        unsigned long long v = std::strtoull(s, nullptr, 10);
        cap = clamp_capacity(static_cast<size_t>(v));
    });
    return cap;
}

inline const char* plan_kind(const EvalProgram& ep) { return ep.simple ? "simple" : "ast"; }

// The cache is hidden behind these statics and guarded by a mutex.
inline std::unordered_map<std::string, std::pair<std::shared_ptr<CacheEntry>, std::list<std::string>::iterator>>& cache_map() {
    static std::unordered_map<std::string, std::pair<std::shared_ptr<CacheEntry>, std::list<std::string>::iterator>> m;
    return m;
}
inline std::list<std::string>& cache_lru() {
    static std::list<std::string> l;
    return l;
}
inline std::mutex& cache_mtx() {
    static std::mutex m;
    return m;
}
inline std::atomic<size_t>& cache_cap() {
    static std::atomic<size_t> c{ env_capacity() }; // initialize from env once
    return c;
}

// API: compile (cached)
inline std::shared_ptr<const CacheEntry> compile_where_expr_cached(const std::string& raw, size_t capacity = 0 /*0?use current*/) {
    std::lock_guard<std::mutex> lock(cache_mtx());
    auto& map_ = cache_map();
    auto& lru_ = cache_lru();

    if (capacity == 0) capacity = cache_cap().load();

    const std::string norm = sqlnorm::sql_to_dottalk_where(raw);

    auto hit = map_.find(norm);
    if (hit != map_.end()) {
        lru_.erase(hit->second.second);
        lru_.push_front(norm);
        hit->second.second = lru_.begin();
        return hit->second.first;
    }

    auto entry = std::make_shared<CacheEntry>();
    entry->plan = std::make_shared<EvalProgram>();
    entry->fields = extract_field_names(norm);

    auto tks = stok(norm);
    if (parse_simple_chain(tks, entry->plan->clauses, entry->plan->ops)) {
        entry->plan->simple = true;
    } else {
        auto cr = compile_where(norm);
        if (!cr) throw std::runtime_error("WHERE syntax error: " + cr.error);
        entry->plan->prog = std::move(cr.program);
    }

    lru_.push_front(norm);
    map_[norm] = {entry, lru_.begin()};

    while (capacity > 0 && map_.size() > capacity) {
        const std::string& old = lru_.back();
        auto it = map_.find(old);
        if (it != map_.end()) map_.erase(it);
        lru_.pop_back();
    }
    return entry;
}

// API: run
inline bool run_program(const EvalProgram& ep, xbase::DbArea& A) {
    if (ep.simple) {
        return eval_chain(ep.clauses, ep.ops, A);
    } else {
        auto rv = dottalk::expr::glue::make_record_view(A);
        return ep.prog->eval(rv);
    }
}

// ---------- Cache management (dev) ----------
inline void cache_clear() {
    std::lock_guard<std::mutex> lock(cache_mtx());
    cache_map().clear();
    cache_lru().clear();
}
struct CacheStats { size_t size=0, capacity=0; };
inline CacheStats cache_stats() {
    std::lock_guard<std::mutex> lock(cache_mtx());
    return CacheStats{ cache_map().size(), cache_cap().load() };
}
inline void cache_set_capacity(size_t new_cap) {
    std::lock_guard<std::mutex> lock(cache_mtx());
    cache_cap().store(clamp_capacity(new_cap));
    // Optional: shrink immediately
    auto& map_ = cache_map();
    auto& lru_ = cache_lru();
    while (map_.size() > cache_cap().load()) {
        const std::string& old = lru_.back();
        auto it = map_.find(old);
        if (it != map_.end()) map_.erase(it);
        lru_.pop_back();
    }
}

} // namespace where_eval




