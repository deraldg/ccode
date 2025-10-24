#pragma once
#include "xindex/index_backend.hpp"
#include <map>
#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <fstream>

namespace xindex {

// MVP B+Tree backend:
//
// Internals use std::multimap<Key, Recno> for ordered keys + duplicates.
// File format is a compact binary dump so tags persist between runs.
// Later, replace internals with a real paged B+tree (node split/merge) without
// changing the public IIndexBackend interface.

class BPlusTreeBackend : public IIndexBackend {
public:
    using Key   = std::vector<uint8_t>;
    using Recno = uint32_t;

    BPlusTreeBackend() = default;
    ~BPlusTreeBackend() override { close(); }

    // --- IIndexBackend ---
    bool open(const std::string& path) override;  // path to .idx file
    void close() override;

    // write‑path mutations
    void insert(const std::vector<uint8_t>& key, uint32_t recno) override;
    void erase (const std::vector<uint8_t>& key, uint32_t recno) override;

    // lookups / navigation
    bool seekEq(const std::vector<uint8_t>& key, uint32_t* out_recno) const override;
    std::unique_ptr<Cursor> scan(const Range& r) const override;

    // maintenance/persistence
    bool save() const override;
    bool load() override;

private:
    // NOTE: If your IIndexBackend uses different names/signatures for any method,
    // adapt here. (This class is intentionally small so renames are trivial.)

    struct EntryLess {
        bool operator()(const Key& a, const Key& b) const noexcept {
            // lexicographic byte compare
            if (a.size() != b.size())
                return a < b; // std::vector lexicographic handles size as a tie-breaker
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
        }
    };

    // Ordered container for duplicates (multiple recnos per equal key)
    using Map = std::multimap<Key, Recno, EntryLess>;

    // Cursor over a contiguous range [lo, hi] (with inclusive flags)
    class MapCursor : public Cursor {
    public:
        MapCursor(Map::const_iterator it, Map::const_iterator end)
            : it_(it), end_(end) {}
        bool valid() const override { return it_ != end_; }
        void next() override { if (it_ != end_) ++it_; }
        void prev() override { /* MVP: not implemented for forward-only scan */ }
        const std::vector<uint8_t>& key() const override { return it_->first; }
        uint32_t recno() const override { return it_->second; }
    private:
        Map::const_iterator it_, end_;
    };

    // helpers
    static std::string ensureExt(const std::string& path);

    // persistence helpers
    bool saveToFile(const std::string& file) const;
    bool loadFromFile(const std::string& file);

    // storage
    std::string path_;
    Map map_;
};

} // namespace xindex
