#pragma once
#include "xindex/key_common.hpp"
#include "xindex/index_backend.hpp"

#include <iosfwd>
#include <map>
#include <memory>
#include <string>

namespace xindex {

// File-backed stub that currently keeps data in-memory and can load/save it
// in a trivial binary format. Interface-compatible with IIndexBackend.
class BPlusTreeBackend : public IIndexBackend {
public:
    BPlusTreeBackend() = default;
    ~BPlusTreeBackend() override = default;

    bool open(const std::string& path) override;
    void close() override;

    void setFingerprint(std::uint32_t /*fp*/) override { /* noop for now */ }
    bool wasStale() const override { return stale_; }

    void rebuild() override { /* stub */ }

    void upsert(const Key& key, RecNo rec) override;
    void erase (const Key& key, RecNo rec) override;

    std::unique_ptr<Cursor> seek(const Key& key) const override;
    std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const override;

private:
    using Map = std::multimap<Key, RecNo, KeyLess>;
    Map map_{};

    std::string path_{};
    bool        stale_{false};

    // small helpers implemented in the .cpp
    static void     write_u32(std::ostream& os, std::uint32_t v);
    static uint32_t read_u32 (std::istream& is);

    bool load(const std::string& path);
    bool saveToFile(const std::string& path) const;

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
