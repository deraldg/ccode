// src/cli/cmd_struct.cpp — STRUCT [ALL] [INDEX]
// Adds best‑effort CDX tag enumeration (legacy INX is fully supported).

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <cstring>

#include "xbase.hpp"
#include "order_state.hpp"
#include "workareas.hpp"

using xbase::DbArea;

// INX reader (same as STATUS)
static std::vector<std::string> legacy_inx_read_all_tags(const std::string& path) {
    std::vector<std::string> names;
    std::ifstream f(path, std::ios::binary);
    if (!f) return names;
    uint8_t m0=0,m1=0,m2=0,m3=0, ver=0, flags=0;
    f.read(reinterpret_cast<char*>(&m0),1);
    f.read(reinterpret_cast<char*>(&m1),1);
    f.read(reinterpret_cast<char*>(&m2),1);
    f.read(reinterpret_cast<char*>(&m3),1);
    if (!f || m0!='1' || m1!='I' || m2!='N' || m3!='X') return names;
    f.read(reinterpret_cast<char*>(&ver),1);
    f.read(reinterpret_cast<char*>(&flags),1);
    while (true) {
        uint16_t nameLen=0; uint32_t keyCount=0;
        f.read(reinterpret_cast<char*>(&nameLen),2);
        if (!f) break;
        if (nameLen==0 || nameLen>4096) break;
        std::string name(nameLen, '\0');
        f.read(&name[0], nameLen);
        if (!f) break;
        names.push_back(name);
        f.read(reinterpret_cast<char*>(&keyCount),4);
        if (!f) break;
        for (uint32_t i=0;i<keyCount;i++) {
            uint16_t klen=0; f.read(reinterpret_cast<char*>(&klen),2);
            if (!f) { break; }
            f.seekg(klen, std::ios::cur);
            uint32_t recno=0; f.read(reinterpret_cast<char*>(&recno),4);
            if (!f) { break; }
        }
        if (!f) break;
    }
    return names;
}

// CDX best‑effort tag name guesser (same heuristic as STATUS)
static std::vector<std::string> cdx_guess_tag_names(const std::string& path) {
    std::vector<std::string> out;
    std::ifstream f(path, std::ios::binary);
    if (!f) return out;
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto is_print = [](unsigned char c){ return c>=32 && c<127; };
    for (size_t i=0; i<buf.size();) {
        if (!is_print(buf[i])) { ++i; continue; }
        size_t j=i; size_t maxlen = std::min<size_t>(10, buf.size()-i);
        size_t len=0;
        while (len<maxlen && is_print(buf[j])) { ++len; ++j; }
        if (len>=1 && len<=10) {
            bool left_ok  = (i==0) || (buf[i-1]==0 || buf[i-1]==32);
            bool right_ok = (j>=buf.size()) || (buf[j]==0 || buf[j]==32);
            if (left_ok && right_ok) {
                std::string s(reinterpret_cast<const char*>(&buf[i]), len);
                bool good = std::isalpha(static_cast<unsigned char>(s[0]))!=0;
                for (char c : s) {
                    if (!(std::isalnum(static_cast<unsigned char>(c)) || c=='_')) { good=false; break; }
                }
                if (good) if (std::find(out.begin(), out.end(), s)==out.end()) out.push_back(s);
            }
            i = j+1;
        } else {
            ++i;
        }
        if (out.size() > 32) break;
    }
    return out;
}

static inline bool ends_with_icase(const std::string& s, const char* ext) {
    if (s.size() < std::strlen(ext)) return false;
    std::string tail = s.substr(s.size()-std::strlen(ext));
    std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c){ return std::tolower(c); });
    std::string e(ext);
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c){ return std::tolower(c); });
    return tail == e;
}

static void print_dbf_struct(DbArea& A) {
    std::cout << "Fields (" << A.fieldCount() << ")\n";
    std::cout << "  " << std::left << std::setw(2) << "#" 
              << " " << std::left << std::setw(12) << "Name"
              << " " << std::left << std::setw(5) << "Type"
              << " " << std::right << std::setw(5) << "Len"
              << " " << std::right << std::setw(5) << "Dec"
              << "\n";
    int idx = 1;
    for (const auto& f : A.fields()) {
        std::string t(1, f.type);
        std::cout << "  " << std::left << std::setw(2) << idx++
                  << " " << std::left << std::setw(12) << f.name
                  << " " << std::left << std::setw(5) << t
                  << " " << std::right << std::setw(5) << int(f.length)
                  << " " << std::right << std::setw(5) << int(f.decimals)
                  << "\n";
    }
}

static void print_indexes_for(DbArea& A) {
    std::string orderSpec;
    try { orderSpec = orderstate::hasOrder(A) ? orderstate::orderName(A) : std::string(); } catch (...) {}
    if (orderSpec.empty()) {
        std::cout << "Index file: (none)\n";
        std::cout << "Tags: 0\n";
        return;
    }

    std::cout << "Index file: " << orderSpec << "\n";
    std::vector<std::string> tags;
    if (ends_with_icase(orderSpec, ".inx")) {
        tags = legacy_inx_read_all_tags(orderSpec);
    } else if (ends_with_icase(orderSpec, ".cdx")) {
        tags = cdx_guess_tag_names(orderSpec);
    }

    std::cout << "Tags: " << tags.size() << "\n";
    for (const auto& t : tags) std::cout << "  - " << t << "\n";
}

static void print_area_struct(std::size_t slot, DbArea& A, bool alsoIndex) {
    const char* label = workareas::name(slot);
    std::cout << "\n-- Area " << slot;
    if (label && *label) std::cout << " [" << label << "]";
    std::cout << " --\n";

    if (!A.isOpen()) { std::cout << "  (closed)\n"; return; }

    std::cout << "File: " << A.filename() << "\n";
    print_dbf_struct(A);
    if (alsoIndex) print_indexes_for(A);
}

// Entry point
void cmd_STRUCT(xbase::DbArea& /*current*/, std::istringstream& args) {
    bool all=false, withIndex=false;
    std::string tok;
    while (args >> tok) {
        for (auto& c : tok) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (tok == "ALL") all = true; else if (tok == "INDEX") withIndex = true;
    }

    if (!all) {
        auto cur = workareas::current_slot();
        if (cur >= workareas::count()) { std::cout << "STRUCT: no current area.\n"; return; }
        if (auto* a = workareas::at(cur)) print_area_struct(cur, *a, withIndex);
        else std::cout << "STRUCT: no current area.\n";
        return;
    }

    const std::size_t n = workareas::count();
    for (std::size_t slot = 0; slot < n; ++slot) {
        if (auto* a = workareas::at(slot)) if (a->isOpen()) print_area_struct(slot, *a, withIndex);
    }
}
