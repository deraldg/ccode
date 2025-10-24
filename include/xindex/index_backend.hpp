#pragma once
#include "xindex/key_common.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace xindex {

// Simple cursor interface returning a stream of (Key, RecNo)
struct Cursor {
    virtual ~Cursor() = default;
    // Position to the first record in the cursor's range
    // Returns false if empty. On true, fills out parameters.
    virtual bool first(Key& outKey, RecNo& outRec) = 0;
    // Advance; same return contract as first()
    virtual bool next(Key& outKey, RecNo& outRec) = 0;
};

struct IIndexBackend {
    virtual ~IIndexBackend() = default;

    virtual bool open(const std::string& path) = 0;
    virtual void close() = 0;

    virtual void setFingerprint(std::uint32_t /*fp*/) = 0;
    virtual bool wasStale() const = 0;

    virtual void rebuild() = 0;

    virtual void upsert(const Key& key, RecNo rec) = 0;
    virtual void erase(const Key& key, RecNo rec) = 0;

    virtual std::unique_ptr<Cursor> seek(const Key& key) const = 0;
    virtual std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const = 0;
};

} // namespace xindex
