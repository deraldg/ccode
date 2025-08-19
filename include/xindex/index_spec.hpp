#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>

namespace xindex {

struct IndexSpec {
    std::string tag;
    std::vector<std::string> fields;
    bool ascending = true;
    bool unique = false;
};

inline std::string ix_up(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}
inline std::string ix_trim(std::string s) {
    auto issp = [](unsigned char c){ return std::isspace(c)!=0; };
    while (!s.empty() && issp((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    return s;
}

} // namespace xindex
