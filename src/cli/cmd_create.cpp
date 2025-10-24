// src/cli/cmd_create.cpp
#include "xbase.hpp"
#include "order_state.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---- tiny utils ----
static std::string trim(std::string s) {
    auto sp = [](unsigned char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; };
    while (!s.empty() && sp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && sp((unsigned char)s.back()))  s.pop_back();
    return s;
}
static std::string to_upper(std::string s){
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}
static inline void put_u16(std::ostream& o, uint16_t v) {
    o.put((char)(v & 0xFF)); o.put((char)((v >> 8) & 0xFF));
}
static inline void put_u32(std::ostream& o, uint32_t v) {
    o.put((char)(v & 0xFF)); o.put((char)((v >> 8) & 0xFF));
    o.put((char)((v >> 16) & 0xFF)); o.put((char)((v >> 24) & 0xFF));
}
static std::string replace_ext_local(const std::string& p, const char* newext){
    auto dot = p.find_last_of('.');
    if (dot == std::string::npos) return p + newext;
    return p.substr(0, dot) + newext;
}

// ---- in-file field spec we control ----
struct FieldSpec { std::string name; char type{}; uint8_t len{0}; uint8_t dec{0}; };

// split top-level by ',' or ';'
static std::vector<std::string> split_top(const std::string& inside) {
    std::vector<std::string> out;
    int depth = 0; size_t start = 0;
    for (size_t i=0;i<inside.size();++i){
        char c = inside[i];
        if (c=='(') ++depth;
        else if (c==')' && depth>0) --depth;
        else if ((c==',' || c==';') && depth==0) {
            out.emplace_back(trim(inside.substr(start, i-start)));
            start = i+1;
        }
    }
    if (start < inside.size()) out.emplace_back(trim(inside.substr(start)));
    return out;
}

// parse "NAME C(20)" | "GPA N(3,2)" | "BIRTHDATE D" | "ACTIVE L" | "NOTES M"
static bool parse_one(const std::string& part, FieldSpec& fs){
    std::istringstream ps(part);
    std::string fname, typeAndParams;
    if (!(ps >> fname >> typeAndParams)) return false;
    if (fname.empty() || typeAndParams.empty()) return false;

    fs.name = fname;
    char T = (char)std::toupper((unsigned char)typeAndParams[0]);

    // pull "(...)" if present
    std::string params;
    auto lp = typeAndParams.find('(');
    if (lp != std::string::npos) {
        auto rp = typeAndParams.find_last_of(')');
        if (rp == std::string::npos || rp < lp) return false;
        params = typeAndParams.substr(lp+1, rp-lp-1);
    }

    fs.type = T;
    fs.dec  = 0;

    if (T=='C') {
        if (params.empty()) return false;
        fs.len = (uint8_t)std::stoi(params);
        if (fs.len == 0) return false;
    } else if (T=='N') {
        if (params.empty()) return false;
        auto comma = params.find(',');
        if (comma == std::string::npos) {
            fs.len = (uint8_t)std::stoi(params);
            fs.dec = 0;
        } else {
            fs.len = (uint8_t)std::stoi(params.substr(0, comma));
            fs.dec = (uint8_t)std::stoi(params.substr(comma+1));
        }
        if (fs.len == 0 || fs.dec > fs.len) return false;
    } else if (T=='D') { fs.len = 8; }
    else if (T=='L')   { fs.len = 1; }
    else if (T=='M')   { fs.len = 10; } // memo pointer in DBF
    else return false;

    return true;
}

// parse CREATE args into (tableName, fields)
static bool parse_field_list(std::istringstream& args, std::vector<FieldSpec>& out, std::string& tableName) {
    if (!(args >> tableName)) return false;

    char ch;
    if (!(args >> ch) || ch != '(') return false;

    std::string inside; int depth = 1; char c;
    while (args.get(c)) {
        if (c=='(') ++depth;
        else if (c==')') { if (--depth==0) break; }
        inside.push_back(c);
    }
    if (depth != 0) return false;

    auto parts = split_top(inside);
    out.clear();
    for (auto& p : parts) {
        if (p.empty()) continue;
        FieldSpec fs;
        if (!parse_one(p, fs)) return false;
        out.push_back(fs);
    }
    return !out.empty();
}

// write a minimal dBASE III header; create .dbt if any memo fields
static bool create_dbf_basic(const std::string& path, const std::vector<FieldSpec>& fields) {
    bool hasMemo = false;
    uint16_t recLen = 1; // delete flag
    for (auto& f : fields) {
        recLen += f.len;
        if (f.type == 'M') hasMemo = true;
    }
    uint16_t hdrLen = (uint16_t)(32 + fields.size()*32 + 1);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    // --- 32-byte file header ---
    out.put(hasMemo ? (char)0x83 : (char)0x03); // version
    std::time_t t = std::time(nullptr); std::tm lt{};
    #if defined(_WIN32)
      localtime_s(&lt, &t);
    #else
      lt = *std::localtime(&t);
    #endif
    out.put((char)lt.tm_year);              // years since 1900
    out.put((char)(lt.tm_mon + 1));         // month
    out.put((char)lt.tm_mday);              // day
    put_u32(out, 0);                        // record count
    put_u16(out, hdrLen);                   // header length
    put_u16(out, recLen);                   // record length
    for (int i=0;i<20;++i) out.put(0);      // reserved

    // --- field descriptors: 32 bytes each ---
    for (auto& f : fields) {
        char name11[11] = {0};
        std::string fn = f.name; if (fn.size() > 11) fn.resize(11);
        std::memcpy(name11, fn.c_str(), fn.size());
        out.write(name11, 11);              // name[11]
        out.put((char)f.type);              // type
        put_u32(out, 0);                    // data address (unused here)
        out.put((char)f.len);               // length
        out.put((char)f.dec);               // decimal count
        // the next 14 bytes are reserved in many minimal structs; zero them
        for (int i=0;i<14;++i) out.put(0);
    }
    out.put((char)0x0D);                    // header terminator
    out.close();

    if (hasMemo) {
        std::string dbt = replace_ext_local(path, ".dbt");
        std::ofstream m(dbt, std::ios::binary | std::ios::trunc);
        if (!m) return false;
        uint32_t next = 1;                  // next free block
        m.write(reinterpret_cast<const char*>(&next), 4);
        // pad first block to 512 bytes
        m.seekp(512 - 1); char z = 0; m.write(&z, 1);
        m.close();
    }
    return true;
}

} // namespace

// public entry point
void cmd_CREATE(xbase::DbArea& area, std::istringstream& args)
{
    std::vector<FieldSpec> fields;
    std::string table;
    if (!parse_field_list(args, fields, table)) {
        std::cout << "CREATE <name> (<FIELD TYPE(len[,dec]) ...>)\n"
                  << "Types: C(n), N(n[,d]), D, L, M\n";
        return;
    }

    if (area.isOpen()) { orderstate::clearOrder(area); area.close(); }

    // Use your helper (present in your tree) to normalize extension.
    const std::string path = xbase::dbNameWithExt(table);

    if (!create_dbf_basic(path, fields)) {
        std::cout << "CREATE failed: could not write file\n"; return;
    }
    area.open(path);

    bool hasMemo = std::any_of(fields.begin(), fields.end(), [](const FieldSpec& f){ return f.type=='M'; });
    std::cout << "Created " << path << (hasMemo ? " (+ .dbt)" : "") << "\n";
    std::cout << "Opened "  << area.filename() << " with " << area.recCount() << " records.\n";
}
