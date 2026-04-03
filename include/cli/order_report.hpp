#pragma once
// order_report.hpp ? unified order readout helpers for AREA/STATUS.
//
// Goal: make it obvious how the current index orders the table.
// - AREA:
//     Index file  : <file> (key)
//     Active tag  : <tag | (none) | (single-order)>
// - STATUS/WSREPORT:
//     Order       : ASCEND|DESCEND
//     Index file  : <file> (key)
//     Active tag  : <tag | (none) | (single-order)>
//     Sort        : <key>
//
// Notes:
// * For CNX, "key" is the tag name.
// * For CDX, "key" is the tag name.
// * For INX, we try in order:
//     1) orderstate::activeTag(area)
//     2) INX v1 header "1INX" key/expression name (if present)
//     3) filename suffix "<stem>_<TAG>.inx"  ? TAG
//     4) "(single-order)"
//
// Drop-in: include this in cmd_area.cpp and STATUS/WSREPORT implementation and call:
//    orderreport::print_area_one_line(std::cout, A);
//    orderreport::print_status_block(std::cout, A);
//
// Dependencies: xbase.hpp, order_state.hpp

#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <algorithm>
#include <vector>
#include "xbase.hpp"
#include "order_state.hpp"

namespace orderreport {

// --------- small utils -------------------------------------------------------
inline bool ieq(char a, char b){ return std::toupper((unsigned char)a)==std::toupper((unsigned char)b); }
inline bool iends_with(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    for (size_t i=0;i<suf.size();++i){
        if (!ieq(s[s.size()-suf.size()+i], suf[i])) return false;
    }
    return true;
}
inline bool is_cdx_path(const std::string& cont) { return iends_with(cont, ".cdx"); }
inline bool is_cnx_path(const std::string& cont) { return iends_with(cont, ".cnx"); }
inline bool is_inx_path(const std::string& cont) { return iends_with(cont, ".inx"); }

inline std::string upper_copy(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

inline std::string basename_no_ext(const std::string& p){
    size_t slash = p.find_last_of("/\\");
    std::string base = (slash==std::string::npos) ? p : p.substr(slash+1);
    size_t dot = base.find_last_of('.');
    return (dot==std::string::npos) ? base : base.substr(0, dot);
}

// Derive an INX tag name from filename "name_TAG.inx" => "TAG".
// If no underscore before extension, return empty string.
inline std::string derive_inx_tag_from_filename(const std::string& cont){
    const std::string stem = basename_no_ext(cont);
    size_t us = stem.find_last_of('_');
    if (us==std::string::npos) return std::string{};
    std::string tag = stem.substr(us+1);
    if (tag.empty()) return std::string{};
    return upper_copy(tag);
}

// Try to read INX v1 ("1INX") header to get the key/expression name.
// Layout (as used by your tools):
//   magic[4] = "1INX"
//   uint16 skip
//   uint16 nameLen
//   name[nameLen] (UTF-8 text for key/expression)
// Returns empty string if not available.
inline std::string probe_inx_key_name(const std::string& cont){
    std::ifstream f(cont, std::ios::binary);
    if (!f) return std::string{};
    char magic[4]; if (!f.read(magic,4)) return std::string{};
    if (std::string(magic,4)!="1INX") return std::string{};
    auto rd_u16 = [&](uint16_t& v)->bool {
        unsigned char b[2]; if (!f.read((char*)b,2)) return false;
        v = (uint16_t)(b[0] | (b[1]<<8)); return true;
    };
    uint16_t skip=0, nlen=0;
    if (!rd_u16(skip) || !rd_u16(nlen)) return std::string{};
    if (nlen==0 || nlen>8192) return std::string{};
    std::vector<char> buf(nlen);
    if (!f.read(buf.data(), nlen)) return std::string{};
    std::string name(buf.begin(), buf.end());
    // trim whitespace
    auto ltrim=[&](std::string& s){ s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c){return !std::isspace(c);})); };
    auto rtrim=[&](std::string& s){ s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c){return !std::isspace(c);} ).base(), s.end()); };
    ltrim(name); rtrim(name);
    return name;
}

// Provide a single "key by" string for display.
inline std::string compute_key_by(const std::string& cont, const std::string& activeTag) {
    if (is_cnx_path(cont)) {
        // CNX: tag is the key
        return activeTag.empty() ? std::string("(none)") : activeTag;
    }
    if (is_cdx_path(cont)) {
        // CDX: tag is the key
        return activeTag.empty() ? std::string("(none)") : activeTag;
    }

    if (is_inx_path(cont)) {
        // Prefer explicit tag if engine provided
        if (!activeTag.empty()) return activeTag;
        // Try header probe
        std::string hdr = probe_inx_key_name(cont);
        if (!hdr.empty()) return hdr;
        // Fall back to filename suffix
        std::string inf = derive_inx_tag_from_filename(cont);
        if (!inf.empty()) return inf;
        return std::string("(single-order)");
    }
    // Unknown container kind
    return activeTag.empty() ? std::string("(none)") : activeTag;
}

// Decide whether to show "(key)" next to the index filename.
inline bool show_key_parenthetical(const std::string& key){
    if (key.empty()) return false;
    if (key == "(none)") return false;
    if (key == "(single-order)") return true; // still useful context
    return true;
}

// --------- public printers ---------------------------------------------------

// One-line area summary.
inline void print_area_one_line(std::ostream& os, xbase::DbArea& a) {
    using namespace orderstate;

    if (!a.isOpen()) { os << "  (no file open)\n"; return; }

    if (!hasOrder(a)) {
        os << "  Order: NATURAL\n";
        return;
    }

    const std::string cont = orderName(a);
    const bool asc = isAscending(a);
    const std::string tag = activeTag(a);
    const std::string keyline = compute_key_by(cont, tag);

    os << "  Order: " << (asc ? "ASCEND" : "DESCEND") << "\n";
    os << "  Index file  : " << cont;
    if (show_key_parenthetical(keyline)) os << " (" << keyline << ")";
    os << "\n";
    os << "  Active tag  : " << (tag.empty() ? "(none)" : tag) << "\n";
}

// Multi-line status block.
inline void print_status_block(std::ostream& os, xbase::DbArea& a) {
    using namespace orderstate;

    os << "Order       : " << (isAscending(a) ? "ASCEND" : "DESCEND") << "\n";
    if (!hasOrder(a)) {
        os << "Index file  : (none)\n";
        os << "Active tag  : \n";
        os << "Sort        : \n";
        return;
    }

    const std::string cont = orderName(a);
    const std::string tag  = activeTag(a);
    const std::string key  = compute_key_by(cont, tag);

    os << "Index file  : " << cont;
    if (show_key_parenthetical(key)) os << " (" << key << ")";
    os << "\n";
    os << "Active tag  : " << (tag.empty() ? "(none)" : tag) << "\n";
    os << "Sort        : " << key << "\n";
}

} // namespace orderreport



