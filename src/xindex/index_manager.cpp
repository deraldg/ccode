#pragma once
#include "xindex/index_backend.hpp"

#include <memory>
#include <optional>
#include <string>

namespace xindex {

class IndexManager {
public:
    enum class Backend { InMemory, FileBPlus };

    explicit IndexManager(Backend b = Backend::InMemory);
    ~IndexManager();

    bool open(const std::string& path);
    void close();

    void upsert(const Key& key, RecNo rec);
    void erase (const Key& key, RecNo rec);

    // Exact-match lookup; returns one RecNo if present.
    std::optional<RecNo> seek(const Key& key) const;

    // Cursors for iteration.
    std::unique_ptr<Cursor> cursorForKey(const Key& key) const;
    std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const;

    bool wasStale() const;
    void rebuild();

private:
    std::unique_ptr<IIndexBackend> impl_;
};

} // namespace xindex
