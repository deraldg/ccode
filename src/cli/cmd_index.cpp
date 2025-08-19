#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>

#include "xbase.hpp"
#include "xindex/index_spec.hpp"
#include "xindex/attach.hpp"
#include "xindex/dbarea_adapt.hpp"

using xindex::IndexSpec;

static std::string up(std::string s){ for(auto& c:s) c=(char)std::toupper((unsigned char)c); return s; }
static std::string trim(std::string s){
    auto sp=[](unsigned char c){return std::isspace(c)!=0;};
    while(!s.empty()&&sp((unsigned char)s.front())) s.erase(s.begin());
    while(!s.empty()&&sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static std::vector<std::string> split_csv(std::string s){
    std::vector<std::string> out; size_t i=0;
    while(i<s.size()){
        size_t c=s.find(',',i);
        out.push_back(trim(c==std::string::npos? s.substr(i): s.substr(i,c-i)));
        if(c==std::string::npos) break; i=c+1;
    }
    out.erase(std::remove_if(out.begin(),out.end(),[](auto& t){return t.empty();}),out.end());
    return out;
}

void cmd_INDEX(xbase::DbArea& area, std::istringstream& iss) {
    std::string tok;
    if(!(iss>>tok) || up(tok)!="ON"){
        std::puts("Syntax: INDEX ON <field[,field...]> [TAG <name>] [ASCEND|DESCEND] [UNIQUE]");
        return;
    }

    std::string rest; std::getline(iss, rest);
    std::string uprest = up(rest);

    size_t posTag  = uprest.find(" TAG ");
    size_t posAsc  = uprest.find(" ASCEND");
    size_t posDesc = uprest.find(" DESCEND");
    size_t posUni  = uprest.find(" UNIQUE");

    size_t optStart = std::string::npos;
    for(size_t p : {posTag,posAsc,posDesc,posUni}) if(p!=std::string::npos) optStart = optStart==std::string::npos? p: std::min(optStart,p);

    auto fields = split_csv(optStart==std::string::npos? rest : rest.substr(0,optStart));
    if(fields.empty()){ std::puts("INDEX: no fields provided."); return; }

    IndexSpec spec;
    spec.fields    = fields;
    spec.tag       = fields.front();
    spec.ascending = (posDesc==std::string::npos);
    spec.unique    = (posUni !=std::string::npos);

    if(posTag!=std::string::npos){
        size_t p = posTag + 5;
        while(p<rest.size() && std::isspace((unsigned char)rest[p])) ++p;
        size_t q=p; while(q<rest.size() && !std::isspace((unsigned char)rest[q])) ++q;
        if(q>p) spec.tag = rest.substr(p,q-p);
    }

    auto& mgr = xindex::ensure_manager(area);
    auto& tag = mgr.ensure_tag(spec);
    (void)tag;
    mgr.set_active(spec.tag);
    mgr.set_direction(spec.ascending);
    mgr.save(xindex::db_current_dbf_path(area));

    std::printf("Built index TAG %s on %zu field(s). Order: %s.\n",
        spec.tag.c_str(), spec.fields.size(), spec.ascending? "ASC":"DESC");
}
