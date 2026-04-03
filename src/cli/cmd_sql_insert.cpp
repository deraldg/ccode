#include "xbase.hpp"
#include "textio.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static inline std::string dt_trim(std::string s){
    auto sp=[](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while(!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while(!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static inline std::string up(std::string s){ for(char& c: s) c=(char)std::toupper((unsigned char)c); return s; }

static bool parse_ident_list(const std::string& src, size_t& i, std::vector<std::string>& out){
    const size_t n = src.size();
    auto ws=[&]{ while(i<n && std::isspace((unsigned char)src[i])) ++i; };
    ws();
    if(i>=n || src[i] != '(') return false;
    ++i; ws();
    for(;;){
        if(i>=n) return false;
        size_t j=i;
        if(!(std::isalpha((unsigned char)src[j]) || src[j]=='_')) return false;
        ++j; while(j<n && (std::isalnum((unsigned char)src[j]) || src[j]=='_')) ++j;
        out.push_back(up(src.substr(i, j-i)));
        i=j; ws();
        if(i<n && src[i]==','){ ++i; ws(); continue; }
        if(i<n && src[i]==')'){ ++i; break; }
        return false;
    }
    return true;
}

static std::string parse_value_token(const std::string& src, size_t& i){
    const size_t n = src.size();
    auto ws=[&]{ while(i<n && std::isspace((unsigned char)src[i])) ++i; };
    ws(); if(i>=n) return {};
    if(src[i]=='\'' || src[i]=='"'){
        char q = src[i++];
        std::string out;
        while(i<n){
            char c = src[i++];
            if(c==q){
                if(i<n && src[i]==q){ out.push_back(q); ++i; continue; }
                break;
            }
            out.push_back(c);
        }
        return out;
    }
    size_t j=i; while(j<n && !std::isspace((unsigned char)src[j]) && src[j]!=',' && src[j]!=')') ++j;
    std::string t = src.substr(i, j-i); i=j; return t;
}

static bool parse_values_tuple(const std::string& src, size_t& i, std::vector<std::string>& out){
    const size_t n = src.size();
    auto ws=[&]{ while(i<n && std::isspace((unsigned char)src[i])) ++i; };
    ws(); if(i>=n || src[i] != '(') return false;
    ++i;
    for(;;){
        std::string v = parse_value_token(src, i);
        out.push_back(v);
        ws();
        if(i<n && src[i]==','){ ++i; continue; }
        if(i<n && src[i]==')'){ ++i; break; }
        return false;
    }
    return true;
}

void cmd_SQL_INSERT(xbase::DbArea& A, std::istringstream& iss){
    if(!A.isOpen()){ std::cout<<"No file open\n"; return; }

    std::string rest; std::getline(iss, rest);
    rest = dt_trim(rest);
    if(rest.empty()){ std::cout<<"INSERT syntax\n"; return; }

    auto defs = A.fields();
    auto idx_of = [&](const std::string& upname)->int{
        for(size_t f=0; f<defs.size(); ++f){
            std::string t = defs[f].name;
            for(char& c: t) c = (char)std::toupper((unsigned char)c);
            if(t == upname) return int(f)+1;
        }
        return -1;
    };

    int inserted = 0;

    if(!rest.empty() && rest[0]=='('){
        size_t i=0;
        std::vector<std::string> fields;
        if(!parse_ident_list(rest, i, fields)){ std::cout<<"INSERT: bad field list\n"; return; }
        while(i<rest.size() && std::isspace((unsigned char)rest[i])) ++i;
        if(up(rest.substr(i,6))!="VALUES"){ std::cout<<"INSERT: expected VALUES\n"; return; }
        i+=6;
        for(;;){
            std::vector<std::string> vals;
            if(!parse_values_tuple(rest, i, vals)){ std::cout<<"INSERT: bad VALUES tuple\n"; return; }
            if(vals.size()!=fields.size()){ std::cout<<"INSERT: field/value count mismatch\n"; return; }
            if(!A.appendBlank() || !A.readCurrent()){ std::cout<<"INSERT: APPEND failed\n"; return; }
            for(size_t k=0;k<fields.size();++k){
                int idx = idx_of(fields[k]);
                if(idx<0){ std::cout<<"INSERT: unknown field "<<fields[k]<<"\n"; return; }
                A.set(idx, vals[k]);
            }
            if(!A.writeCurrent()){ std::cout<<"INSERT: write failed\n"; return; }
            ++inserted;
            while(i<rest.size() && std::isspace((unsigned char)rest[i])) ++i;
            if(i<rest.size() && rest[i]==','){ ++i; continue; }
            break;
        }
    }else{
        std::istringstream ss(rest);
        std::string pair;
        std::vector<std::pair<int,std::string>> assigns;
        while(std::getline(ss, pair, ',')){
            auto eq = pair.find('=');
            if(eq==std::string::npos){ std::cout<<"INSERT: expected name=value\n"; return; }
            auto trim=[](std::string s){
                while(!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
                while(!s.empty() && (s.back()==' '||s.back()=='\t')) s.pop_back();
                return s;
            };
            std::string name = up(trim(pair.substr(0,eq)));
            std::string val  = trim(pair.substr(eq+1));
            if(!val.empty() && (val.front()=='\''||val.front()=='"')){
                if(val.size()>=2 && (val.back()=='\''||val.back()=='"')) val = val.substr(1, val.size()-2);
            }
            int idx = idx_of(name);
            if(idx<0){ std::cout<<"INSERT: unknown field "<<name<<"\n"; return; }
            assigns.push_back({idx, val});
        }
        if(!A.appendBlank() || !A.readCurrent()){ std::cout<<"INSERT: APPEND failed\n"; return; }
        for(auto& p: assigns) A.set(p.first, p.second);
        if(!A.writeCurrent()){ std::cout<<"INSERT: write failed\n"; return; }
        ++inserted;
    }

    std::cout<<inserted<<" row(s) inserted\n";
}



