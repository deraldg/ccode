// src/cli/foxref.cpp
#include "foxref.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace {

// uppercase copy (ASCII; fine for our CLI tokens)
std::string up(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::toupper(c)));
    return out;
}

// case-insensitive equality
bool ieq(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) !=
            std::toupper(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

// case-insensitive substring test
bool icontains(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return true;
    const std::string H = up(hay);
    const std::string N = up(needle);
    return H.find(N) != std::string::npos;
}

} // namespace

namespace foxref {

const Item* find(std::string_view name) {
    const auto& cat = catalog();
    for (const auto& it : cat) {
        if (ieq(it.name, name)) return &it;
    }
    return nullptr;
}

std::vector<const Item*> search(std::string_view token) {
    std::vector<const Item*> out;
    if (token.empty()) return out;

    const auto& cat = catalog();
    for (const auto& it : cat) {
        if (icontains(it.name, token) ||
            (it.syntax && icontains(it.syntax, token)) ||
            (it.summary && icontains(it.summary, token))) {
            out.push_back(&it);
        }
    }
    return out;
}

} // namespace foxref
