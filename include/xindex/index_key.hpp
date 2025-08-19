#pragma once
#include <string>
#include <variant>
#include <vector>
#include <cstdio>

namespace xindex {

using KeyAtom = std::variant<std::string, double>;

struct IndexKey {
    std::vector<KeyAtom> parts;
};

inline int cmp_atom(KeyAtom const& a, KeyAtom const& b) {
    if (a.index() == b.index()) {
        if (std::holds_alternative<std::string>(a)) {
            auto const& sa = std::get<std::string>(a);
            auto const& sb = std::get<std::string>(b);
            if (sa < sb) return -1;
            if (sa > sb) return 1;
            return 0;
        } else {
            double da = std::get<double>(a);
            double db = std::get<double>(b);
            if (da < db) return -1;
            if (da > db) return 1;
            return 0;
        }
    }
    auto to_str = [](KeyAtom const& v)->std::string{
        if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
        char buf[64]; std::snprintf(buf, sizeof(buf), "%.15g", std::get<double>(v)); return std::string(buf);
    };
    auto sa = to_str(a), sb = to_str(b);
    if (sa < sb) return -1;
    if (sa > sb) return 1;
    return 0;
}

inline int cmp_key(IndexKey const& a, IndexKey const& b) {
    size_t n = a.parts.size() < b.parts.size() ? a.parts.size() : b.parts.size();
    for (size_t i = 0; i < n; ++i) {
        int c = cmp_atom(a.parts[i], b.parts[i]);
        if (c != 0) return c;
    }
    if (a.parts.size() < b.parts.size()) return -1;
    if (a.parts.size() > b.parts.size()) return 1;
    return 0;
}

} // namespace xindex
