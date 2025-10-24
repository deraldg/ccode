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
#include <filesystem>

// --- DotTalk++ expression engine ---
#include "dottalk/expr/api.hpp"
#include "dottalk/expr/ast.hpp"
#include "dottalk/expr/eval.hpp"

using namespace xbase;

namespace {

// ----- local DBF field meta we already use -----
struct FieldMeta { std::string name; char type{}; int length{0}; int decimal{0}; size_t offset{0}; };

static std::string up(std::string s){ std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); }); return s; }
static std::string trim(std::string s){
    auto issp=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
    while(!s.empty()&&issp((unsigned char)s.front())) s.erase(s.begin());
    while(!s.empty()&&issp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static bool digits_only(const std::string& s){ return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){return std::isdigit(c)!=0;}); }
static bool is_quoted(const std::string& s){ return s.size()>=2 && ((s.front()=='"'&&s.back()=='"')||(s.front()=='\''&&s.back()=='\'')); }
static std::string strip_quotes(std::string s){ if(is_quoted(s)) s=s.substr(1,s.size()-2); return s; }
// strip trailing inline comments outside quotes: supports && (FoxPro) and // (C++-style)
static std::string strip_inline_comments_value(std::string s) {
    bool inq = false; char q = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];

        // toggle quoting state
        if (c == '"' || c == '\'') {
            if (!inq) { inq = true; q = c; }
            else if (c == q) { inq = false; }
            continue;
        }

        if (inq) continue; // ignore comment sentinels inside quotes

        // FoxPro-style: &&
        if (c == '&' && i + 1 < s.size() && s[i + 1] == '&') {
            s.resize(i);
            break;
        }
        // C++-style: //
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            s.resize(i);
            break;
        }
    }
    // final trim (so "2.97   //..." becomes "2.97")
    auto issp = [](unsigned char ch){ return ch==' '||ch=='\t'||ch=='\r'||ch=='\n'; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static bool parse_logical(std::string s, char& outTF){
    s=strip_quotes(s); s=up(trim(s));
    if(s.empty()||s=="BLANK"||s=="NULL"){ outTF=' '; return true; }
    if(s==".T."||s=="T"||s=="Y"||s=="YES"||s=="1"||s=="TRUE"){ outTF='T'; return true; }
    if(s==".F."||s=="F"||s=="N"||s=="NO"||s=="0"||s=="FALSE"){ outTF='F'; return true; }
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

static std::string dbt_path_from_dbf(const std::string& dbf) {
    std::filesystem::path p(dbf); p.replace_extension(".dbt"); return p.string();
}
static void write_le32(char* dst, uint32_t v) {
    dst[0]=(char)(v&0xFF); dst[1]=(char)((v>>8)&0xFF); dst[2]=(char)((v>>16)&0xFF); dst[3]=(char)((v>>24)&0xFF);
}

// append text to .dbt
static uint32_t dbt_append_text(const std::string& dbtPath, const std::string& text){
    const size_t BlockSize=512;
    {
        std::fstream init(dbtPath,std::ios::in|std::ios::out|std::ios::binary);
        if(!init){ std::ofstream create(dbtPath,std::ios::binary); std::string hdr(BlockSize,'\0'); create.write(hdr.data(),hdr.size()); }
        else{ init.seekp(0,std::ios::end); auto sz=(size_t)init.tellp(); if(sz<BlockSize){ std::string pad(BlockSize-sz,'\0'); init.write(pad.data(),pad.size()); } }
    }
    std::fstream io(dbtPath,std::ios::in|std::ios::out|std::ios::binary);
    if(!io) return 0;
    io.seekp(0,std::ios::end);
    auto fileSize=(uint64_t)io.tellp();
    uint32_t startBlock=(uint32_t)(fileSize/BlockSize);
    if(startBlock==0) startBlock=1;
    std::string payload=text; payload.push_back(0x1A);
    size_t rem=payload.size()%BlockSize;
    if(rem) payload.append(BlockSize-rem,'\0');
    io.seekp((std::streamoff)startBlock*(std::streamoff)BlockSize,std::ios::beg);
    io.write(payload.data(),(std::streamsize)payload.size());
    io.flush(); if(!io) return 0;
    return startBlock;
}

// evaluate numeric robustly
static double eval_number_or_bool(const std::unique_ptr<dottalk::expr::Expr>& e,
                                  const dottalk::expr::RecordView& rv){
    using namespace dottalk::expr;
    if(auto ar=dynamic_cast<const Arith*>(e.get())) return ar->evalNumber(rv);
    if(auto ln=dynamic_cast<const LitNumber*>(e.get())) return ln->v;
    if(auto fr=dynamic_cast<const FieldRef*>(e.get())){
        if(auto n=rv.get_field_num(fr->name)) return *n;
        return 0.0;
    }
    return e->eval(rv)?1.0:0.0;
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
    rest = strip_inline_comments_value(rest);
    std::string value=trim(rest);
    if(value.empty()){ std::cout<<"Usage: REPLACE <field> WITH <value>\n"; return; }

    HeaderRec hdr{}; std::vector<FieldMeta> metas;
    if(!read_header_and_fields(a, hdr, metas)){ std::cout<<"Failed to read header\n"; return; }

    int fi=find_field(metas, up(field));
    if(fi<0){ std::cout<<"Unknown field: "<<field<<"\n"; return; }
    const auto& m=metas[fi];

    const long rec=a.recno();
    const std::streamoff recStart = (std::streamoff)hdr.data_start
                                  + (std::streamoff)((rec-1)*hdr.cpr)
                                  + (std::streamoff)1; // delete flag (1 byte)

    std::streampos pos = (std::streampos)recStart + (std::streamoff)m.offset;

    // --- Build a RecordView for THIS record so expressions can read fields numerically ---
    dottalk::expr::RecordView rv;
    rv.get_field_str = [&](std::string_view nm) -> std::string {
        int idx = find_field(metas, up(std::string(nm)));
        if (idx < 0) return {};
        const auto &fm = metas[(size_t)idx];
        std::ifstream in(a.name(), std::ios::binary);
        if (!in) return {};
        std::string cell((size_t)fm.length, ' ');
        in.seekg(recStart + (std::streamoff)fm.offset, std::ios::beg);
        in.read(cell.data(), (std::streamsize)cell.size());
        // return trimmed string cell
        auto s = cell;
        // left/right trim spaces
        while(!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
        while(!s.empty() && std::isspace((unsigned char)s.back()))  s.pop_back();
        return s;
    };
    rv.get_field_num = [&](std::string_view nm) -> std::optional<double> {
        int idx = find_field(metas, up(std::string(nm)));
        if (idx < 0) return std::nullopt;
        const auto &fm = metas[(size_t)idx];
        std::ifstream in(a.name(), std::ios::binary);
        if (!in) return std::nullopt;
        std::string cell((size_t)fm.length, ' ');
        in.seekg(recStart + (std::streamoff)fm.offset, std::ios::beg);
        in.read(cell.data(), (std::streamsize)cell.size());
        // Trim for parsing
        auto s = cell;
        while(!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
        while(!s.empty() && std::isspace((unsigned char)s.back()))  s.pop_back();

        // Only numeric/logical/date likely to be used in arithmetic; try stod
        try {
            if (s.empty()) return std::nullopt;
            size_t used=0;
            double v = std::stod(s, &used);
            if (used == s.size()) return v;
        } catch(...) {}
        // Logical: T/F/space
        if (fm.type=='L'){
            if (!s.empty()){
                char c = (char)std::toupper((unsigned char)s[0]);
                if (c=='T') return 1.0;
                if (c=='F') return 0.0;
            }
            return 0.0;
        }
        // Dates (YYYYMMDD) are not directly numeric; treat as null
        return std::nullopt;
    };

    // MEMO path
    if(m.type=='M'){
        std::string vup=up(strip_quotes(value));
        bool clear=vup.empty()||vup=="BLANK"||vup=="NULL";
        std::fstream io(a.name(),std::ios::in|std::ios::out|std::ios::binary);
        if(!io){ std::cout<<"Open failed: cannot write file\n"; return; }
        char memoPtr[10]{}; uint32_t block=0;
        if(!clear){
            std::string memoText=strip_quotes(value);
            block=dbt_append_text(dbt_path_from_dbf(a.name()),memoText);
            if(block==0){ std::cout<<"Memo write failed.\n"; return; }
            write_le32(memoPtr,block);
        }
        io.seekp(pos,std::ios::beg); io.write(memoPtr,sizeof memoPtr); io.flush();
        if(!io){ std::cout<<"Write failed.\n"; return; }
        std::cout<<"Replaced MEMO "<<m.name<<" at recno "<<rec<<" (block "<<block<<").\n";
        try{ a.gotoRec(rec); }catch(...) {}
        return;
    }

    // --- evaluate RHS (numeric-first) ---
    if(!is_quoted(value)){
        try{
            auto cr = dottalk::expr::compile_where(value);
            if(!cr.program){ std::cout<<"REPLACE error: "<<cr.error<<"\n"; return; }
            double num = eval_number_or_bool(cr.program, rv);
            value = std::to_string(num);
        }catch(const std::exception& e){ std::cout<<"REPLACE error: "<<e.what()<<"\n"; return; }
        catch(...){ std::cout<<"REPLACE error: evaluation failed.\n"; return; }
    }else{
        value=strip_quotes(value);
    }

    // non-MEMO write
    bool ok=false; std::string cell=format_value(m,value,ok);
    if(!ok){
        std::cout<<"Value not valid for field "<<m.name<<" (type "<<m.type<<", len "<<m.length<<(m.decimal?(","+std::to_string(m.decimal)):std::string())<<")\n";
        return;
    }
    std::fstream io(a.name(),std::ios::in|std::ios::out|std::ios::binary);
    if(!io){ std::cout<<"Open failed: cannot write file\n"; return; }
    io.seekp(pos,std::ios::beg);
    io.write(cell.data(),(std::streamsize)cell.size());
    io.flush();
    if(!io){ std::cout<<"Write failed.\n"; return; }

    std::cout<<"Replaced "<<m.name<<" at recno "<<rec<<".\n";
    try{ a.gotoRec(rec); }catch(...) {}
}
