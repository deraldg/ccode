#pragma once
// memo_sidecar_v1/include/memo/dtx_format.hpp
//
// On-disk layout for DotTalk++ memo sidecar files (.dtx).
// v1 is intentionally simple: a fixed 4KB header and an append-mostly object area.
// All integers are little-endian. Alignment: 8 bytes for object bodies.
//
// NOTE: This header only defines on-disk structs and constants. High-level logic
// lives in memostore.hpp/cpp.

#include <cstdint>
#include <array>

namespace xbase::memo {

constexpr std::array<char,4> DTX_MAGIC = {'D','T','X','1'};
constexpr uint16_t DTX_VERSION = 0x0001;
constexpr uint32_t DTX_DEFAULT_BLOCK_SIZE = 4096;
constexpr uint32_t DTX_HEADER_SIZE        = 4096;
constexpr uint32_t DTX_ALIGN              = 8;     // object bodies padded to 8 bytes

// Header is fixed 4KB. Keep room for future fields.
// --- at the top with the other includes
#include <cstddef>

// ... inside namespace xbase::memo {

// Header is fixed 4KB. Keep room for future fields.
struct DtxHeader {
    char     magic[4];          // "DTX1"
    uint16_t version;           // 0x0001
    uint16_t reserved0;         // align (kept)
    uint32_t block_size;        // typically 4096

    uint32_t pad0;              // <<< NEW: explicit pad so next u64 is 8-byte aligned

    uint64_t next_object_id;    // next id to assign (monotonic)
    uint64_t free_bytes;        // total bytes in free-list (best effort)
    uint64_t used_bytes;        // total payload bytes (sum of object lengths)
    uint64_t object_count;      // count of allocated objects (live, not tombstoned)
    uint64_t root_freelist_off; // file offset to free-list head (0 if none)
    uint32_t header_crc32;      // crc of header with this field zeroed
    uint32_t reserved1;         // align

    // 64 bytes of fixed fields above; tail fills to 4096
    uint8_t  reserved2[DTX_HEADER_SIZE - 64];
};

    static_assert(offsetof(DtxHeader, reserved2) == 64, "DtxHeader reserved2 must start at 64");
    static_assert(sizeof(DtxHeader) == DTX_HEADER_SIZE, "DtxHeader must be 4KB");

// Each object on disk is stored as: [u32 length][u32 crc][bytes...][padding to 8-byte]
// This struct describes the fixed prefix; the body follows it.
struct DtxObjectPrefix {
    uint32_t length; // number of bytes of memo body
    uint32_t crc32;  // CRC32 over the memo body (not including padding)
};

// Free list node (persisted) — simple singly-linked list (v1).
// Larger strategies can be added in future versions.
struct DtxFreeNode {
    uint64_t size;      // free span length, in bytes (including node header itself)
    uint64_t next_off;  // file offset of next free node (0 if none)
    // Body of the free span follows (unused bytes)
};

} // namespace xbase::memo
