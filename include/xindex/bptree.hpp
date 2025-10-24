#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <limits>
#include <iosfwd>

namespace xindex {

struct Key {
    std::vector<uint8_t> bytes;
    bool operator<(const Key& o) const noexcept { return bytes < o.bytes; }
    bool operator==(const Key& o) const noexcept { return bytes == o.bytes; }
};

class BPlusTree {
public:
    explicit BPlusTree(int order = 64) : order_(std::max(8, order)) { newRootLeaf_(); }

    void clear() { nodes_.clear(); root_ = newRootLeaf_(); }

    void insert(const std::vector<uint8_t>& k, int32_t v) {
        Key key{k};
        auto split = insertRec_(root_, key, v);
        if (split.has_value()) {
            int parent = newInternal_();
            nodes_[parent].keys = {split->firstKey};
            nodes_[parent].children = {root_, split->newRight};
            nodes_[parent].isLeaf = false;
            root_ = parent;
        }
    }

    void erase(const std::vector<uint8_t>& k, int32_t v) {
        Key key{k};
        eraseRec_(root_, key, v);
        if (!nodes_[root_].isLeaf && nodes_[root_].children.size() == 1)
            root_ = nodes_[root_].children[0];
    }

    std::optional<int32_t> seekGE(const std::vector<uint8_t>& target) const {
        Key key{target};
        int n = root_;
        while (!nodes_[n].isLeaf) {
            const auto& K = nodes_[n].keys;
            auto it = std::lower_bound(K.begin(), K.end(), key);
            size_t idx = static_cast<size_t>(it - K.begin());
            if (idx >= nodes_[n].children.size()) idx = nodes_[n].children.size() - 1;
            n = nodes_[n].children[idx];
        }
        const auto& K = nodes_[n].keys;
        const auto& V = nodes_[n].values;
        auto it = std::lower_bound(K.begin(), K.end(), key);
        if (it == K.end()) {
            int next = nodes_[n].nextLeaf;
            if (next < 0) return std::nullopt;
            if (!nodes_[next].keys.empty()) return nodes_[next].values[0];
            return std::nullopt;
        }
        size_t i = static_cast<size_t>(it - K.begin());
        return V[i];
    }

    void save(std::ostream& os) const {
        writeU32_(os, 'B'<<24 | 'P'<<16 | 'T'<<8 | '1');
        writeI32_(os, order_);
        writeI32_(os, root_);
        writeI32_(os, static_cast<int32_t>(nodes_.size()));
        for (const auto& n : nodes_) {
            writeU8_(os, n.isLeaf ? 1 : 0);
            writeI32_(os, n.nextLeaf);
            writeI32_(os, static_cast<int32_t>(n.keys.size()));
            for (const auto& k : n.keys) {
                writeI32_(os, static_cast<int32_t>(k.bytes.size()));
                if (!k.bytes.empty())
                    os.write(reinterpret_cast<const char*>(k.bytes.data()),
                             static_cast<std::streamsize>(k.bytes.size()));
            }
            if (n.isLeaf) {
                for (int32_t v : n.values) writeI32_(os, v);
            } else {
                writeI32_(os, static_cast<int32_t>(n.children.size()));
                for (int c : n.children) writeI32_(os, c);
            }
        }
    }

    void load(std::istream& is) {
        nodes_.clear();
        uint32_t magic = readU32_(is);
        if (magic != (static_cast<uint32_t>('B')<<24 | static_cast<uint32_t>('P')<<16 |
                      static_cast<uint32_t>('T')<<8  | static_cast<uint32_t>('1')))
            throw std::runtime_error("BPlusTree: bad magic");
        order_ = readI32_(is);
        root_  = readI32_(is);
        int32_t N = readI32_(is);
        nodes_.resize(N);
        for (int i = 0; i < N; ++i) {
            auto& n = nodes_[i];
            n.isLeaf  = (readU8_(is) != 0);
            n.nextLeaf = readI32_(is);
            int32_t kc = readI32_(is);
            n.keys.resize(kc);
            for (int j = 0; j < kc; ++j) {
                int32_t L = readI32_(is);
                if (L < 0) throw std::runtime_error("BPlusTree: bad key length");
                n.keys[j].bytes.resize(static_cast<size_t>(L));
                if (L) is.read(reinterpret_cast<char*>(n.keys[j].bytes.data()), L);
            }
            if (n.isLeaf) {
                n.values.resize(kc);
                for (int j = 0; j < kc; ++j) n.values[j] = readI32_(is);
            } else {
                int32_t cc = readI32_(is);
                n.children.resize(cc);
                for (int j = 0; j < cc; ++j) n.children[j] = readI32_(is);
            }
        }
        if (root_ < 0 || root_ >= static_cast<int>(nodes_.size()))
            throw std::runtime_error("BPlusTree: bad root id");
    }

private:
    struct Node {
        bool isLeaf{true};
        std::vector<Key> keys;
        std::vector<int> children;   // internal only
        std::vector<int32_t> values; // leaf only
        int nextLeaf{-1};            // leaf chain
    };

    int order_;
    std::vector<Node> nodes_;
    int root_{0};

    int newRootLeaf_() { nodes_.push_back(Node{}); root_ = static_cast<int>(nodes_.size()) - 1; return root_; }
    int newLeaf_()     { nodes_.push_back(Node{}); return static_cast<int>(nodes_.size()) - 1; }
    int newInternal_() { nodes_.push_back(Node{}); nodes_.back().isLeaf = false; return static_cast<int>(nodes_.size()) - 1; }

    struct SplitRet { Key firstKey; int newRight; };

    std::optional<SplitRet> insertRec_(int nId, const Key& key, int32_t val) {
        Node& n = nodes_[nId];
        if (n.isLeaf) {
            auto it = std::lower_bound(n.keys.begin(), n.keys.end(), key);
            size_t pos = static_cast<size_t>(it - n.keys.begin());
            n.keys.insert(it, key);
            n.values.insert(n.values.begin() + static_cast<long>(pos), val);
            if (static_cast<int>(n.keys.size()) > order_) return splitLeaf_(nId);
            return std::nullopt;
        } else {
            auto it = std::lower_bound(n.keys.begin(), n.keys.end(), key);
            size_t idx = static_cast<size_t>(it - n.keys.begin());
            if (idx >= n.children.size()) idx = n.children.size() - 1;
            auto s = insertRec_(n.children[idx], key, val);
            if (!s) return std::nullopt;
            n.keys.insert(n.keys.begin() + static_cast<long>(idx), s->firstKey);
            n.children.insert(n.children.begin() + static_cast<long>(idx + 1), s->newRight);
            if (static_cast<int>(n.children.size()) > order_ + 1) return splitInternal_(nId);
            return std::nullopt;
        }
    }

    std::optional<SplitRet> splitLeaf_(int nId) {
        Node& L = nodes_[nId];
        int total = static_cast<int>(L.keys.size());
        int mid = total / 2;
        int RId = newLeaf_();
        Node& R = nodes_[RId];
        R.keys.assign(L.keys.begin() + mid, L.keys.end());
        R.values.assign(L.values.begin() + mid, L.values.end());
        L.keys.resize(mid);
        L.values.resize(mid);
        R.nextLeaf = L.nextLeaf;
        L.nextLeaf = RId;
        return SplitRet{ R.keys.front(), RId };
    }

    std::optional<SplitRet> splitInternal_(int nId) {
        Node& P = nodes_[nId];
        int midKeyIdx = static_cast<int>(P.keys.size()) / 2;
        Key upKey = P.keys[midKeyIdx];
        int RId = newInternal_();
        Node& R = nodes_[RId];
        R.keys.assign(P.keys.begin() + (midKeyIdx + 1), P.keys.end());
        P.keys.resize(midKeyIdx);
        R.children.assign(P.children.begin() + (midKeyIdx + 1), P.children.end());
        P.children.resize(midKeyIdx + 1);
        return SplitRet{ upKey, RId };
    }

    bool eraseRec_(int nId, const Key& key, int32_t val) {
        Node& n = nodes_[nId];
        if (n.isLeaf) {
            auto it = std::lower_bound(n.keys.begin(), n.keys.end(), key);
            if (it == n.keys.end()) return false;
            size_t i = static_cast<size_t>(it - n.keys.begin());
            while (i < n.keys.size() && !(key < n.keys[i]) && !(n.keys[i] < key)) {
                if (n.values[i] == val) {
                    n.keys.erase(n.keys.begin() + static_cast<long>(i));
                    n.values.erase(n.values.begin() + static_cast<long>(i));
                    return true;
                }
                ++i;
            }
            return false;
        } else {
            auto it = std::lower_bound(n.keys.begin(), n.keys.end(), key);
            size_t idx = static_cast<size_t>(it - n.keys.begin());
            if (idx >= n.children.size()) idx = n.children.size() - 1;
            return eraseRec_(n.children[idx], key, val);
        }
    }

    static void writeU8_(std::ostream& os, uint8_t v) { os.put(static_cast<char>(v)); }
    static void writeI32_(std::ostream& os, int32_t v) {
        uint8_t b[4] = {
            static_cast<uint8_t>((v >> 24) & 0xFF),
            static_cast<uint8_t>((v >> 16) & 0xFF),
            static_cast<uint8_t>((v >>  8) & 0xFF),
            static_cast<uint8_t>( v        & 0xFF)
        };
        os.write(reinterpret_cast<const char*>(b), 4);
    }
    static void writeU32_(std::ostream& os, uint32_t v) { writeI32_(os, static_cast<int32_t>(v)); }
    static uint8_t readU8_(std::istream& is) { int c = is.get(); if (c == EOF) throw std::runtime_error("BPlusTree: EOF"); return static_cast<uint8_t>(c); }
    static int32_t readI32_(std::istream& is) {
        uint8_t b[4]; is.read(reinterpret_cast<char*>(b), 4); if (!is) throw std::runtime_error("BPlusTree: EOF");
        return (static_cast<int32_t>(b[0]) << 24) | (static_cast<int32_t>(b[1]) << 16) |
               (static_cast<int32_t>(b[2]) <<  8) |  static_cast<int32_t>(b[3]);
    }
    static uint32_t readU32_(std::istream& is) { return static_cast<uint32_t>(readI32_(is)); }
};

} // namespace xindex
