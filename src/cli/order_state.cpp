#include "order_state.hpp"
#include "xbase.hpp"
#include <unordered_map>
#include <string>
#include <algorithm>

namespace {
    struct State {
        bool has_order = false;
        std::string path;       // container path (INX/IDX/CNX). Never a tag.
        bool ascending = true;
        std::string cnx_tag;    // active CNX tag (UPPER) if path is a .cnx; else empty.
    };
    // Track state per area pointer
    std::unordered_map<const xbase::DbArea*, State> g;

    static bool ends_with_icase(const std::string& s, const std::string& suf) {
        if (s.size() < suf.size()) return false;
        auto it = s.end() - (ptrdiff_t)suf.size();
        for (size_t i = 0; i < suf.size(); ++i) {
            char a = (char)std::tolower((unsigned char)it[i]);
            char b = (char)std::tolower((unsigned char)suf[i]);
            if (a != b) return false;
        }
        return true;
    }

    static std::string to_upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c){ return (char)std::toupper(c); });
        return s;
    }
}

namespace orderstate {

bool hasOrder(const xbase::DbArea& a) {
    auto it = g.find(&a);
    return (it != g.end()) && it->second.has_order && !it->second.path.empty();
}

std::string orderName(const xbase::DbArea& a) {
    auto it = g.find(&a);
    return (it == g.end()) ? std::string() : it->second.path;
}

void setOrder(xbase::DbArea& a, const std::string& path) {
    auto& st = g[&a];
    st.has_order = !path.empty();
    st.path = path;
    // When container changes, clear any previous tag selection.
    st.cnx_tag.clear();
    // Preserve st.ascending as-is.
}

void clearOrder(xbase::DbArea& a) {
    auto& st = g[&a];
    st.has_order = false;
    st.path.clear();
    st.cnx_tag.clear();
    st.ascending = true;
}

void setAscending(xbase::DbArea& a, bool asc) {
    g[&a].ascending = asc;
}

bool isAscending(const xbase::DbArea& a) {
    auto it = g.find(&a);
    return (it == g.end()) ? true : it->second.ascending;
}

void setActiveTag(xbase::DbArea& a, const std::string& tagUpper) {
    auto& st = g[&a];
    if (!st.path.empty() && ends_with_icase(st.path, ".cnx")) {
        st.cnx_tag = to_upper(tagUpper);
    } else {
        st.cnx_tag.clear(); // not a CNX container; ignore
    }
}

std::string activeTag(const xbase::DbArea& a) {
    auto it = g.find(&a);
    if (it == g.end()) return {};
    const auto& st = it->second;
    if (!st.path.empty() && ends_with_icase(st.path, ".cnx")) {
        return st.cnx_tag;
    }
    return {};
}

bool isCnx(const xbase::DbArea& a) {
    auto it = g.find(&a);
    if (it == g.end()) return false;
    return ends_with_icase(it->second.path, ".cnx");
}

} // namespace orderstate
