#include "browse_format.hpp"
#include "xbase.hpp"

#include <sstream>
#include <string>

namespace dottalk::browse::format {

static std::string pad(std::string s, size_t width, bool right_align){
    if (s.size() > width) s = s.substr(0, width);
    if (s.size() < width){
        std::string spaces(width - s.size(), ' ');
        return right_align ? (spaces + s) : (s + spaces);
    }
    return s;
}

std::string tuple_pretty(::xbase::DbArea& db){
    const auto& defs = db.fields();
    std::ostringstream line; line << "; TUPLE: ";
    for (size_t i = 0; i < defs.size(); ++i){
        const auto& f = defs[i];
        std::string v = db.get((int)i + 1);
        if (!v.empty() && (v == "<NULL>" || v == "(null)")) v.clear();

        size_t width = 10; bool right = false;
        switch (f.type){
            case 'C': width = f.length > 0 ? (size_t)f.length : v.size(); right = false; break;
            case 'N':
            case 'F': width = f.length > 0 ? (size_t)f.length : (v.empty() ? 1 : v.size()); right = true; break;
            case 'D': width = f.length > 0 ? (size_t)f.length : 8; right = true; break;
            case 'L': width = 1; right = true; if (!v.empty()) v = v.substr(0,1); break;
            case 'M':
            default:  width = f.length > 0 ? (size_t)f.length : 10; right = false; break;
        }

        line << pad(v, width, right);
        if (i + 1 < defs.size()) line << " | ";
    }
    return line.str();
}

std::string tuple_raw(::xbase::DbArea& db){
    const auto& defs = db.fields();
    std::ostringstream line;
    for (size_t i = 0; i < defs.size(); ++i) line << db.get((int)i + 1);
    return line.str();
}

} // namespace dottalk::browse::format
