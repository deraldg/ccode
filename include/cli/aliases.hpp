#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <algorithm>

namespace cli {

class AliasRegistry {
public:
    struct Item {
        std::string name; // alias name (normalized)
        int area;         // 0..N-1
    };

    // set or update an alias -> area
    void set(std::string key, int area) {
        normalize(key);
        map_[key] = area;
    }

    // remove alias; returns true if it existed
    bool erase(std::string key) {
        normalize(key);
        return map_.erase(key) > 0;
    }

    // resolve alias to area number if present
    std::optional<int> resolve(std::string key) const {
        std::string k = normalized(key);
        auto it = map_.find(k);
        if (it == map_.end()) return std::nullopt;
        return it->second;
    }

    // list all aliases
    std::vector<Item> list() const {
        std::vector<Item> out;
        out.reserve(map_.size());
        for (const auto& kv : map_) out.push_back(Item{kv.first, kv.second});
        std::sort(out.begin(), out.end(),
                  [](const Item& a, const Item& b){ return a.name < b.name; });
        return out;
    }

private:
    static void normalize(std::string& s) {
        for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    }
    static std::string normalized(std::string s) { normalize(s); return s; }

    std::unordered_map<std::string,int> map_;
};

// global singleton
inline AliasRegistry& aliases() {
    static AliasRegistry inst;
    return inst;
}

} // namespace cli
