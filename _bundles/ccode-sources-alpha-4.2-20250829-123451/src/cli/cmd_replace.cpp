// src/cli/cmd_replace.cpp
// DotTalk++ — REPLACE <field> WITH <value>
#include "xbase.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <cmath>

using namespace xbase;

namespace {
    struct FieldMeta { std::string name; char type{}; int length{0}; int decimal{0}; size_t offset{0}; };

    static std::string up(std::string s){ std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::toupper(c); }); return s; }
    static std::string trim(std::string s){
        auto issp=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
        while(!s.empty()&&issp((unsigned char)s.front())) s.erase(s.begin());
        while(!s.empty()&&issp((unsigned char)s.back()))  s.pop_back();
        return s;
    }
    static bool digits_only(const std::string& s){
        return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){return std::isdigit(c)!=0;});
    }
    static std::string strip_quotes(std::string s){
        if(s.size()>=2 && ((s.front()=='"'&&s.back()=='"')||(s.front()=='\''&&s.back()=='\''))) s=s.substr(1,s.size()-2);
        return s;
    }

    static bool parse_logical(std::string s, char& outTF){
        s=strip_quotes(s); s=up(trim(s));
        if(s.empty()||s=="BLANK"||s=="NULL"){ outTF=' '; return true; }
        if(s==".T."||s=="T"||s=="Y"||s=="YES"||s=="1"||s=="TRUE"){ outTF='T'; return true; }
        if(s==".F."||s=="F"||s=="N"||s=="NO" ||s=="0"||s=="FALSE"){ outTF='F'; return true; }
        return false;
    }
    static bool parse_date(std::string s, std::string& yyyymmdd){
        s=strip_quotes(s); s=trim(s); std::string sup=up(s);
        if(s.empty()||sup=="BLANK"||sup=="NULL"){ yyyymmdd=std::string(8,' '); return true; }
        if(digits_only(s)&&s.size()==8){ yyyymmdd=s; return true; }
        std::vector<std::string> parts;
        for(size_t i=0;i<s.size();){ if(std::isdigit((unsigned char)s[i])){ size_t j=i; while(j<s.size()&&std::isdigit((unsigned char)s[j])) ++j; parts.emplace_back(s.substr(i,j-i)); i=j; } else ++i; }
        auto pad2=[](int v){ std::string t=std::to_string(v); return (t.size()==1?"0"+t:t); };
        int Y=0,M=0,D=0;
        if(parts.size()==3){
            if(parts[0].size()==4){ Y=std::stoi(parts[0]); M=std::stoi(parts[1]); D=std::stoi(parts[2]); }
            else if(parts[2].size()==4){ M=std::stoi(parts[0]); D=std::stoi(parts[1]); Y=std::stoi(parts[2]); }
            else return false;
        } else return false;
        if(Y<1||Y>9999||M<1||M>12||D<1||D>31) return false;
        yyyymmdd=std::to_string(Y)+pad2(M)+pad2(D); return true;
    }

    static bool read_header_and_fields(const DbArea& a, HeaderRec& hdr, std::vector<FieldMeta>& metas){
        std::ifstream in(a.name(), std::ios::binary); if(!in) return false;
        in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr)); if(!in) return false;
        metas.clear(); size_t offset=0;
        for(;;){
            FieldRec fr{}; in.read(reinterpret_cast<char*>(&fr), sizeof(fr)); if(!in) return false;
            if((unsigned char)fr.field_name[0]==0x0D) break;
            char namebuf[12]; std::memcpy(namebuf, fr.field_name, 11); namebuf[11]='\0';
            FieldMeta m; m.name=trim(std::string(namebuf)); m.type=fr.field_type;
            m.length=(int)fr.field_length; m.decimal=(int)fr.decimal_places; m.offset=offset;
            metas.push_back(m); offset+= (size_t)m.length; if(metas.size()>1024) break;
        }
        return true;
    }

    static int find_field(const std::vector<FieldMeta>& metas, const std::string& nameUp){
        for(size_t i=0;i<metas.size();++i) if(up(metas[i].name)==nameUp) return (int)i; return -1;
    }

    static std::string format_value(const FieldMeta& m, const std::string& rawIn, bool& ok){
        ok=true; std::string out((size_t)m.length, ' ');
        switch(m.type){
        case 'C': { std::string v=strip_quotes(rawIn); v=trim(v);
                    if(v.size()> (size_t)m.length) v.resize((size_t)m.length);
                    std::copy(v.begin(), v.end(), out.begin()); break; }
        case 'N': { try{
                        std::string t=trim(rawIn); std::string tu=up(strip_quotes(t));
                        if(t.empty()||tu=="BLANK"||tu=="NULL") break; // blank numeric
                        double d=std::stod(t); std::ostringstream oss;
                        if(m.decimal>0){ oss<<std::fixed<<std::setprecision(m.decimal)<<d; }
                        else { long long ll=(long long)std::llround(d); oss<<ll; }
                        std::string v=oss.str(); if(v.size()>(size_t)m.length){ ok=false; return out; }
                        std::copy(v.rbegin(), v.rend(), out.rbegin());
                    }catch(...){ ok=false; } break; }
        case 'L': { char tf=' '; if(!parse_logical(rawIn, tf)){ ok=false; break; } out[0]=tf; break; }
        case 'D': { std::string ymd; if(!parse_date(rawIn, ymd)||ymd.size()!=8||m.length<8){ ok=false; break; }
                    std::copy(ymd.begin(), ymd.end(), out.begin()); break; }
        default: ok=false; break;
        }
        return out;
    }
} // namespace

void cmd_REPLACE(DbArea& a, std::istringstream& iss)
{
    if(!a.isOpen()){ std::cout<<"No table open.\n"; return; }
    if(a.recno()<=0 || a.recno()>a.recCount()){ std::cout<<"Invalid current record.\n"; return; }

    std::string field; if(!(iss>>field)){ std::cout<<"Usage: REPLACE <field> WITH <value>\n"; return; }

    std::string maybeWith; std::streampos afterFieldPos=iss.tellg();
    if(iss>>maybeWith){ if(up(maybeWith)!="WITH"){ iss.clear(); iss.seekg(afterFieldPos); } }

    std::string rest; std::getline(iss, rest);
    std::string value=trim(rest);
    if(value.empty()){ std::cout<<"Usage: REPLACE <field> WITH <value>\n"; return; }

    HeaderRec hdr{}; std::vector<FieldMeta> metas;
    if(!read_header_and_fields(a, hdr, metas)){ std::cout<<"Failed to read header\n"; return; }

    int fi = find_field(metas, up(field));
    if(fi<0){ std::cout<<"Unknown field: "<<field<<"\n"; return; }
    const auto& m = metas[fi];

    // Guard: MEMO not supported
    if(m.type=='M'){ std::cout<<"Cannot REPLACE MEMO field: "<<m.name<<"\n"; return; }

    bool ok=false; std::string cell = format_value(m, value, ok);
    if(!ok){
        std::cout<<"Value not valid for field "<<m.name
                 <<" (type "<<m.type<<", len "<<m.length
                 <<(m.decimal? ("," + std::to_string(m.decimal)) : std::string())
                 <<")\n";
        return;
    }

    std::fstream io(a.name(), std::ios::in|std::ios::out|std::ios::binary);
    if(!io){ std::cout<<"Open failed: cannot write file\n"; return; }

    const long rec = a.recno();
    std::streampos pos = (std::streampos)hdr.data_start
                       + (std::streamoff)((rec-1)*hdr.cpr)
                       + std::streamoff(1) // delete flag
                       + (std::streamoff)m.offset;

    io.seekp(pos, std::ios::beg);
    io.write(cell.data(), (std::streamsize)cell.size());
    io.flush();
    if(!io){ std::cout<<"Write failed.\n"; return; }

    std::cout<<"Replaced "<<m.name<<" at recno "<<rec<<".\n";
    try{ a.gotoRec(rec); }catch(...) {}
}
