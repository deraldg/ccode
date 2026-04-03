#pragma once
// include/memo64.hpp
//
// DotTalk++ memo64: LMDB-backed 64-bit memo storage for 0x64 tables.
//
// Design:
// - DBF-side M fields in x64 tables store MemoRef64 (uint64_t memo_id)
// - memo_id == 0 means empty memo
// - LMDB environment holds two named DBs:
//     meta   -> control values such as next_memo_id
//     memos  -> memo_id -> [MemoValueHeader + payload]
//
// Scope of v1:
// - text and binary memo read/write
// - no garbage collection yet
// - no compression yet (flag reserved)
// - copy-on-write friendly API

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace memo64 {

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

constexpr std::uint32_t MEMO64_VERSION = 1;

// -----------------------------------------------------------------------------
// Flags
// -----------------------------------------------------------------------------

enum MemoFlags : std::uint32_t
{
    MF_NONE       = 0,
    MF_TEXT       = 1u << 0,
    MF_BINARY     = 1u << 1,
    MF_JSON       = 1u << 2,
    MF_COMPRESSED = 1u << 3,
    MF_DELETED    = 1u << 4
};

// -----------------------------------------------------------------------------
// DBF-side reference
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
struct MemoRef64
{
    std::uint64_t memo_id;   // 0 = empty memo
};
#pragma pack(pop)

static_assert(sizeof(MemoRef64) == 8, "MemoRef64 must be 8 bytes");

// -----------------------------------------------------------------------------
// LMDB value layout
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
struct MemoValueHeader
{
    std::uint32_t flags;
    std::uint32_t reserved;
    std::uint64_t length;   // payload byte count
};
#pragma pack(pop)

static_assert(sizeof(MemoValueHeader) == 16, "MemoValueHeader must be 16 bytes");

// -----------------------------------------------------------------------------
// In-memory metadata / blob
// -----------------------------------------------------------------------------

struct MemoInfo
{
    std::uint64_t memo_id{0};
    std::uint32_t flags{MF_NONE};
    std::uint64_t length{0};
};

struct MemoBlob
{
    MemoInfo info;
    std::vector<std::uint8_t> data;
};

// -----------------------------------------------------------------------------
// Meta keys
// -----------------------------------------------------------------------------

inline constexpr const char* META_FORMAT_VERSION = "format_version";
inline constexpr const char* META_NEXT_MEMO_ID   = "next_memo_id";
inline constexpr const char* META_TABLE_NAME     = "table_name";
inline constexpr const char* META_CREATED_UTC    = "created_utc";

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

inline bool is_empty(MemoRef64 r) noexcept
{
    return r.memo_id == 0;
}

inline bool is_text(std::uint32_t f) noexcept
{
    return (f & MF_TEXT) != 0;
}

inline bool is_binary(std::uint32_t f) noexcept
{
    return (f & MF_BINARY) != 0;
}

inline bool is_compressed(std::uint32_t f) noexcept
{
    return (f & MF_COMPRESSED) != 0;
}

// -----------------------------------------------------------------------------
// Store
// -----------------------------------------------------------------------------

class Store
{
public:
    Store();
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    Store(Store&&) noexcept;
    Store& operator=(Store&&) noexcept;

    bool open(const std::string& env_path, std::string* err = nullptr);
    void close();
    bool is_open() const noexcept;

    // Creates/initializes meta + memos DBs if needed.
    bool ensure_initialized(const std::string& table_name,
                            std::string* err = nullptr);

    // Returns newly allocated memo id, or 0 on failure.
    std::uint64_t allocate_id(std::string* err = nullptr);

    bool put_text(std::uint64_t memo_id,
                  const std::string& text,
                  std::uint32_t flags = MF_TEXT,
                  std::string* err = nullptr);

    bool put_blob(std::uint64_t memo_id,
                  const std::vector<std::uint8_t>& data,
                  std::uint32_t flags = MF_BINARY,
                  std::string* err = nullptr);

    bool get(std::uint64_t memo_id,
             MemoBlob& out,
             std::string* err = nullptr);

    bool exists(std::uint64_t memo_id,
                bool* out_exists = nullptr,
                std::string* err = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> _p;
};

} // namespace memo64