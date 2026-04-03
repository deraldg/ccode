// src/cli/order_state.cpp
#include "cli/order_state.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>

#include "xbase.hpp"

namespace orderstate {

namespace {

struct State {
    std::string container;  // path to .inx/.cnx/.cdx/.isx/.csx (empty => none)
    std::string tag;        // tag name (canonicalized to upper)
    bool        ascending{true};
};

std::mutex g_mtx;
std::unordered_map<const xbase::DbArea*, State> g_state;

static inline std::string lower_copy(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static inline bool ends_with_ci(const std::string& s, const char* suffix) {
    const std::string sl = lower_copy(s);
    const std::string su = lower_copy(std::string(suffix));
    if (sl.size() < su.size()) return false;
    return sl.compare(sl.size() - su.size(), su.size(), su) == 0;
}

static inline std::string canon_tag(std::string s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\0')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\0')) --e;
    s = s.substr(b, e - b);

    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static inline State* find_state_unlocked(const xbase::DbArea& area) {
    auto it = g_state.find(&area);
    return (it == g_state.end()) ? nullptr : &it->second;
}

static inline State& state(const xbase::DbArea& area) {
    return g_state[&area];
}

static inline bool container_supports_tag(const std::string& container_path) {
    return ends_with_ci(container_path, ".cnx") || ends_with_ci(container_path, ".cdx");
}

} // namespace

bool hasOrder(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st && !st->container.empty();
}

std::string orderName(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? st->container : std::string{};
}

void setOrder(xbase::DbArea& area, const std::string& container_path) {
    std::lock_guard<std::mutex> lk(g_mtx);

    State& st = state(area);

    const std::string old_container = st.container;
    const bool changed = (old_container != container_path);

    st.container = container_path;

    if (st.container.empty()) {
        st.tag.clear();
        st.ascending = true;
        return;
    }

    if (changed) {
        st.tag.clear();
    }

    if (!container_supports_tag(st.container)) {
        st.tag.clear();
    }
}

void clearOrder(xbase::DbArea& area) {
    setOrder(area, std::string{});
}

void setAscending(xbase::DbArea& area, bool ascending) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State& st = state(area);
    st.ascending = ascending;
}

bool isAscending(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? st->ascending : true;
}

void setActiveTag(xbase::DbArea& area, const std::string& tag_name) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State& st = state(area);

    if (!container_supports_tag(st.container)) {
        st.tag.clear();
        return;
    }

    st.tag = canon_tag(tag_name);
}

std::string activeTag(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? st->tag : std::string{};
}

bool isInx(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? ends_with_ci(st->container, ".inx") : false;
}

bool isCnx(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? ends_with_ci(st->container, ".cnx") : false;
}

bool isCdx(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? ends_with_ci(st->container, ".cdx") : false;
}

bool isIsx(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? ends_with_ci(st->container, ".isx") : false;
}

bool isCsx(const xbase::DbArea& area) {
    std::lock_guard<std::mutex> lk(g_mtx);
    State* st = find_state_unlocked(area);
    return st ? ends_with_ci(st->container, ".csx") : false;
}

} // namespace orderstate