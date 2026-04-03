// src/cli/cmd_dbarea.cpp
#include "cli/cmd_dbarea.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xbase.hpp"
#include "cli/order_state.hpp"
#include "cli/order_report.hpp"

// engine access: compute area slot without changing DbArea API
extern "C" xbase::XBaseEngine* shell_engine();

namespace {
static inline std::string rtrim_copy(std::string s){
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}
static inline std::string ltrim_copy(std::string s){
    size_t i=0; while (i<s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
static inline std::string trim_copy(std::string s){ return rtrim_copy(ltrim_copy(std::move(s))); }

static inline const char* type_name(char t){
    switch (t){
        case 'C': case 'c': return "Character";
        case 'N': case 'n': return "Numeric";
        case 'D': case 'd': return "Date";
        case 'L': case 'l': return "Logical";
        case 'M': case 'm': return "Memo";
        default:            return "Other";
    }
}
static inline void kv(const std::string& k, const std::string& v, int w=20){
    std::cout << std::left << std::setw(w) << k << ": " << v << "\n";
}
static inline void kv(const std::string& k, int v, int w=20){
    std::cout << std::left << std::setw(w) << k << ": " << v << "\n";
}

// Compute real area slot by identity compare against engine slots.
static int area_slot_of(xbase::DbArea& a){
    auto* eng = shell_engine();
    if (!eng) return -1;
    for (int i = 0; i < xbase::MAX_AREA; ++i){
        if (&eng->area(i) == &a) return i;
    }
    return -1;
}
} // namespace

void cmd_DBAREA(xbase::DbArea& a, std::istringstream& /*iss*/){
    using namespace xbase;

    if (!a.isOpen()){
        std::cout << "DBAREA: no table open.\n";
        return;
    }

    const std::string dbf_abs  = a.filename();
    const std::string logical  = a.name();
    const int32_t recs         = a.recCount();
    const int     reclen       = a.recLength();
    const int     nfields      = a.fieldCount();
    const int32_t crn          = a.recno();
    const auto&   fdefs        = a.fields();

    std::cout << "============================================================\n";
    std::cout << "DBAREA - Current Work Area Summary\n";
    std::cout << "============================================================\n";

    // Area slot
    kv("Area (slot)", area_slot_of(a));

    kv("DBF (abs)",        dbf_abs);
    kv("Logical name",     logical);
    kv("Legacy name()",    logical);

    kv("Records",          static_cast<int>(recs));
    kv("Record length",    reclen);
    kv("recordLength()",   a.recordLength());
    kv("Fields",           nfields);
    kv("Recno",            static_cast<int>(crn));
    kv("Deleted flag",     a.isDeleted()? 1 : 0);

    std::cout << "\nIndex / Order\n";
    std::cout << "-------------\n";

    // Echo index file path explicitly for parity with `area` command
    try {
        const std::string idx_path = orderstate::orderName(a);
        kv("Index file", idx_path.empty() ? std::string("(none)") : idx_path);
    } catch (...) {
        kv("Index file", std::string("(unknown)"));
    }

    // Detailed status (direction, tag, etc.)
    orderreport::print_status_block(std::cout, a);

    std::cout << "\nFields\n";
    std::cout << "------\n";
    if (fdefs.empty()){
        std::cout << "(none)\n";
    } else {
        std::cout << std::left
                  << std::setw(5)  << "#"
                  << std::setw(18) << "Name"
                  << std::setw(10) << "Type"
                  << std::setw(8)  << "Len"
                  << std::setw(8)  << "Dec" << "\n";
        std::cout << std::string(5+18+10+8+8, '-') << "\n";
        for (int i=0; i<static_cast<int>(fdefs.size()); ++i){
            const auto& f = fdefs[i];
            std::cout << std::left
                      << std::setw(5)  << (i+1)
                      << std::setw(18) << trim_copy(f.name)
                      << std::setw(10) << type_name(f.type)
                      << std::setw(8)  << static_cast<int>(f.length)
                      << std::setw(8)  << static_cast<int>(f.decimals)
                      << "\n";
        }
    }

    std::cout << "============================================================\n";
    std::cout.flush();
}
