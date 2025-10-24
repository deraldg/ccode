#pragma once
// memo_sidecar_v1/include/memo/memostore.hpp
//
// Public interface for the memo sidecar store (.dtx).
// Implementation is provided in src/memo/memostore.cpp (stubbed here).

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <functional>

namespace xbase::memo {

struct MemoStat {
    uint64_t length = 0;
    uint64_t created_epoch_ms = 0;
    uint64_t updated_epoch_ms = 0;
    bool     exists = false;
};

struct StoreStat {
    uint64_t used_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t object_count = 0;
    uint64_t next_object_id = 1;
};

// Error types
struct memo_error : std::runtime_error { using std::runtime_error::runtime_error; };
struct io_error   : memo_error { using memo_error::memo_error; };
struct crc_error  : memo_error { using memo_error::memo_error; };
struct not_found  : memo_error { using memo_error::memo_error; };
struct bad_format : memo_error { using memo_error::memo_error; };

// MemoStore is single-writer (process) in v1. Use open/create accordingly.
class MemoStore {
public:
    // Open an existing store; throws if missing or invalid.
    static MemoStore open(const std::string& basepath);   // basepath without extension; ".dtx" implied
    // Create or truncate a store.
    static MemoStore create(const std::string& basepath, uint32_t block_size = 4096);

    MemoStore();                // movable, non-copyable
    MemoStore(MemoStore&&) noexcept;
    MemoStore& operator=(MemoStore&&) noexcept;
    ~MemoStore();

    MemoStore(const MemoStore&) = delete;
    MemoStore& operator=(const MemoStore&) = delete;

    // Main API
    uint64_t put(const std::vector<uint8_t>& data);         // returns object_id
    std::vector<uint8_t> get(uint64_t object_id) const;     // throws not_found
    void update(uint64_t object_id, const std::vector<uint8_t>& data);
    void erase(uint64_t object_id);                          // tombstone / free; id becomes invalid
    MemoStat stat(uint64_t object_id) const;                 // len + timestamps (best effort)

    // Store-wide ops
    void flush();                                            // fsync updates
    StoreStat store_stats() const;
    void memocheck(bool repair = false);                     // validate CRCs, rebuild id map if requested

    // Utilities
    std::string sidecar_path() const;

private:
    struct Impl;
    Impl* impl_; // PIMPL to keep ABI surface minimal
};

// Utility to convert strings to memo bytes (UTF-8 assumed)
inline std::vector<uint8_t> to_bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

} // namespace xbase::memo
