#include "browse_edit.hpp"
#include "xbase.hpp"
#include <iostream>

namespace dottalk::browse::edit {

int field_index_by_name(::xbase::DbArea& db, const std::string& name){
    auto up = [](std::string s){ for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; };
    const auto& defs = db.fields();
    auto needle = up(name);
    for (size_t i = 0; i < defs.size(); ++i){
        if (up(defs[i].name) == needle) return (int)i + 1;
    }
    return 0;
}

void list_fields(::xbase::DbArea& db){
    const auto& defs = db.fields();
    std::cout << "Fields (" << defs.size() << ")\n";
    for (size_t i = 0; i < defs.size(); ++i){
        const auto& f = defs[i];
        std::cout << "  " << (i+1) << ": " << f.name << " [" << f.type << "]";
        if (f.type=='C' || f.type=='N') std::cout << " len=" << f.length;
        if (f.type=='N' && f.decimals>0) std::cout << " dec=" << f.decimals;
        std::cout << "\n";
    }
}

bool commit_staged(::xbase::DbArea& db, StageMap& staged){
    for (auto& kv : staged){
        if (!db.set(kv.first, kv.second)) return false;
    }
    if (!staged.empty() && !db.writeCurrent()) return false;
    staged.clear();
    return true;
}

void discard_staged(StageMap& staged){
    staged.clear();
}

} // namespace dottalk::browse::edit
