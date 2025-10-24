// src/cli/cmd_display.cpp
// DotTalk++ — DISPLAY [<recno>]  (shows current or specified record; includes MEMO text)
#include "xbase.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

using namespace xbase;

namespace {
    struct FieldMeta { std::string name; char type{}; int length{0}; int decimal{0}; size_t offset{0}; };

    static std::string trim(std::string s){
        auto issp=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
        while(!s.empty()&&issp((unsigned char)s.front())) s.erase(s.begin());
        while(!s.empty()&&issp((unsigned char)s.back()))  s.pop_back();
        return s;
    }
    static std::string up(std::string s){ std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::toupper(c); }); return s; }

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

    static std::string dbt_path_from_dbf(const std::string& dbf) {
        std::filesystem::path p(dbf); p.replace_extension(".dbt"); return p.string();
    }
    static uint32_t le32(const unsigned char* p){
        return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
    }

    // Read memo text from .DBT at starting block. Returns empty string on missing/invalid.
    static std::string dbt_read_text(const std::string& dbtPath, uint32_t startBlock) {
        const size_t BlockSize = 512;
        if (startBlock == 0) return std::string();
        std::ifstream in(dbtPath, std::ios::binary);
        if (!in) return std::string();
        // Seek to beginning of the block
        in.seekg((std::streamoff)startBlock * (std::streamoff)BlockSize, std::ios::beg);
        if (!in) return std::string();

        // Read blocks progressively until we see 0x1A terminator or EOF.
        std::string result;
        std::vector<char> buf(BlockSize);
        for (int blocks = 0; blocks < 4096; ++blocks) { // sanity limit
            in.read(buf.data(), (std::streamsize)buf.size());
            std::streamsize got = in.gcount();
            if (got <= 0) break;
            // Find 0x1A
            auto it = std::find(buf.begin(), buf.begin()+got, (char)0x1A);
            if (it != buf.begin()+got) {
                result.append(buf.begin(), it);
                break;
            }
            result.append(buf.begin(), buf.begin()+got);
            if (got < (std::streamsize)buf.size()) break; // short read at end-of-file
        }
        // trim trailing \r or \n that may precede terminator (cosmetic)
        while (!result.empty() && (result.back()=='\r' || result.back()=='\n')) result.pop_back();
        return result;
    }
}

void cmd_DISPLAY(DbArea& a, std::istringstream& iss)
{
    if(!a.isOpen()){ std::cout<<"No table open.\n"; return; }

    long want = 0;
    if (iss >> want) {
        if (want < 1 || want > (long)a.recCount()) { std::cout<<"Invalid record number.\n"; return; }
        try { a.gotoRec((size_t)want); } catch(...) {}
    }
    const long rec = a.recno();
    if(rec<=0 || rec>(long)a.recCount()){ std::cout<<"Invalid current record.\n"; return; }

    HeaderRec hdr{}; std::vector<FieldMeta> metas;
    if(!read_header_and_fields(a, hdr, metas)){ std::cout<<"Failed to read header\n"; return; }

    std::ifstream dbf(a.name(), std::ios::binary);
    if(!dbf){ std::cout<<"Open failed.\n"; return; }

    std::cout<<"Record "<<rec<<"\n";

    // position at start of record data (skip delete flag)
    std::streampos base = (std::streampos)hdr.data_start
                        + (std::streamoff)((rec-1)*hdr.cpr)
                        + std::streamoff(1);

    for (const auto& m : metas) {
        dbf.seekg(base + (std::streamoff)m.offset, std::ios::beg);

        if (m.type == 'M') {
            unsigned char ptr[10]{};
            dbf.read(reinterpret_cast<char*>(ptr), sizeof ptr);
            uint32_t startBlock = le32(ptr);
            std::string memo = dbt_read_text(dbt_path_from_dbf(a.name()), startBlock);
            std::cout<<"  "<<m.name<<" = "<<memo<<"\n";
            continue;
        }

        std::string cell((size_t)m.length, ' ');
        dbf.read(cell.data(), (std::streamsize)cell.size());
        // pretty-print per type
        if (m.type=='C') {
            std::cout<<"  "<<m.name<<" = "<<trim(cell)<<"\n";
        } else if (m.type=='N') {
            std::string s = trim(cell);
            std::cout<<"  "<<m.name<<" = "<<s<<"\n";
        } else if (m.type=='L') {
            char c = cell.empty() ? ' ' : cell[0];
            std::cout<<"  "<<m.name<<" = "<<(c=='T'?'T':(c=='F'?'F':' '))<<"\n";
        } else if (m.type=='D') {
            std::string ymd = cell.size()>=8 ? cell.substr(0,8) : std::string();
            std::cout<<"  "<<m.name<<" = "<<ymd<<"\n";
        } else {
            std::cout<<"  "<<m.name<<" = "<<trim(cell)<<"\n";
        }
    }
}
