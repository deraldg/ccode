#pragma once
#include "xindex/key_common.hpp"
#include "xindex/index_backend.hpp"

#include <map>
#include <memory>

namespace xindex {

// A simple in-memory backend using std::multimap.
// Not a real B+ tree, but compiles/links cleanly and is functionally correct for testing.
class BptMemBackend : public IIndexBackend {
public:
    BptMemBackend() = default;
    ~BptMemBackend() override = default;

    bool open(const std::string& /*path*/) override { stale_ = false; return true; }
    void close() override { /* nothing to release */ }

    void setFingerprint(std::uint32_t /*fp*/) override { /* noop */ }
    bool wasStale() const override { return stale_; }

    void rebuild() override { /* noop: nothing to rebuild for memory-only */ }

    void upsert(const Key& key, RecNo rec) override;
    void erase (const Key& key, RecNo rec) override;

    std::unique_ptr<Cursor> seek(const Key& key) const override;
    std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const override;

private:
    using Map = std::multimap<Key, RecNo, KeyLess>;
    Map map_{};
    bool stale_{false};

    class MapCursor : public Cursor {
    public:
        using It = Map::const_iterator;
        MapCursor(It begin, It end) : begin_(begin), it_(begin), end_(end), started_(false) {}

        bool first(Key& outKey, RecNo& outRec) override;
        bool next (Key& outKey, RecNo& outRec) override;

    private:
        It   begin_;
        It   it_;
        It   end_;
        bool started_;
    };
};

} // namespace xindex
