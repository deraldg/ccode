// dtx_format.hpp
// DotTalk++ native memo sidecar on-disk format definition

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dottalk::memo::dtx {

// -----------------------------------------------------------------------------
// Global format identity
// -----------------------------------------------------------------------------
inline constexpr std::string_view kMagic      = "DTX1";
inline constexpr std::uint16_t    kVersionMaj = 1;
inline constexpr std::uint16_t    kVersionMin = 0;

inline constexpr std::uint32_t kHeaderBytes = 4096;
inline constexpr std::uint32_t kObjectAlign = 16;

enum class ObjectState : std::uint16_t {
    Live      = 0x0001,
    Tombstone = 0x0002
};

enum class PayloadKind : std::uint16_t {
    TextUtf8  = 0x0001,
    Binary    = 0x0002
};

enum class FileFlags : std::uint32_t {
    None            = 0,
    Dirty           = 1u << 0,
    HasTombstones   = 1u << 1,
    HasCrc32        = 1u << 2
};

constexpr inline std::uint32_t operator|(FileFlags a, FileFlags b) noexcept {
    return static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b);
}

constexpr inline bool has_flag(std::uint32_t v, FileFlags f) noexcept {
    return (v & static_cast<std::uint32_t>(f)) != 0;
}

// -----------------------------------------------------------------------------
// In-row memo reference policy
// -----------------------------------------------------------------------------
inline constexpr std::size_t kRefTokenChars = 16;
inline constexpr std::string_view kNullRefToken = "";

// -----------------------------------------------------------------------------
// File header
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct DtxHeader {
    char           magic[4];             // "DTX1"
    std::uint16_t  version_major;        // 1
    std::uint16_t  version_minor;        // 0

    std::uint32_t  header_bytes;         // 4096
    std::uint32_t  object_align;         // 16
    std::uint32_t  file_flags;           // FileFlags bitmask

    std::uint64_t  next_object_id;       // first alloc should typically start at 1
    std::uint64_t  object_count_live;    // advisory
    std::uint64_t  object_count_dead;    // advisory

    std::uint64_t  first_object_offset;  // normally 4096
    std::uint64_t  append_offset;        // next append position

    std::uint64_t  reserved_root1;       // future index/map root
    std::uint64_t  reserved_root2;       // future free-list root
    std::uint64_t  reserved_root3;       // future metadata root

    std::uint32_t  header_crc32;         // optional, 0 if unused in v1
    std::uint32_t  reserved32;

    std::uint8_t   reserved[4004];       // pad to 4096 bytes
};
#pragma pack(pop)

static_assert(sizeof(DtxHeader) == kHeaderBytes, "DtxHeader must be 4096 bytes");

// -----------------------------------------------------------------------------
// Object record header
// -----------------------------------------------------------------------------
#pragma pack(push, 1)
struct DtxObjectHeader {
    char           tag[4];               // "OBJ1"
    std::uint16_t  state;                // ObjectState
    std::uint16_t  kind;                 // PayloadKind

    std::uint64_t  object_id;            // stable DTX object id
    std::uint64_t  payload_bytes;        // number of payload bytes
    std::uint64_t  logical_bytes;        // optional logical text length

    std::uint32_t  payload_crc32;        // optional; 0 if unused
    std::uint32_t  reserved32;

    std::uint64_t  previous_version_of;  // 0 if none
    std::uint64_t  reserved64;
};
#pragma pack(pop)

static_assert(sizeof(DtxObjectHeader) == 56, "DtxObjectHeader must be 56 bytes");

// -----------------------------------------------------------------------------
// Repair / scan constants
// -----------------------------------------------------------------------------
inline constexpr std::string_view kObjectTag = "OBJ1";
inline constexpr std::uint64_t    kInvalidObjectId = 0;
inline constexpr std::uint64_t    kFirstObjectId   = 1;

// -----------------------------------------------------------------------------
// Initialization helpers
// -----------------------------------------------------------------------------
constexpr inline DtxHeader make_default_header() noexcept
{
    DtxHeader h{};

    h.magic[0] = 'D';
    h.magic[1] = 'T';
    h.magic[2] = 'X';
    h.magic[3] = '1';

    h.version_major     = kVersionMaj;
    h.version_minor     = kVersionMin;
    h.header_bytes      = kHeaderBytes;
    h.object_align      = kObjectAlign;
    h.file_flags        = static_cast<std::uint32_t>(FileFlags::None);

    h.next_object_id    = kFirstObjectId;
    h.object_count_live = 0;
    h.object_count_dead = 0;

    h.first_object_offset = kHeaderBytes;
    h.append_offset       = kHeaderBytes;

    h.reserved_root1    = 0;
    h.reserved_root2    = 0;
    h.reserved_root3    = 0;

    h.header_crc32      = 0;
    h.reserved32        = 0;

    return h;
}

constexpr inline DtxObjectHeader make_object_header(std::uint64_t object_id,
                                                    std::uint64_t payload_bytes,
                                                    PayloadKind kind = PayloadKind::TextUtf8) noexcept
{
    DtxObjectHeader h{};

    h.tag[0] = 'O';
    h.tag[1] = 'B';
    h.tag[2] = 'J';
    h.tag[3] = '1';

    h.state               = static_cast<std::uint16_t>(ObjectState::Live);
    h.kind                = static_cast<std::uint16_t>(kind);
    h.object_id           = object_id;
    h.payload_bytes       = payload_bytes;
    h.logical_bytes       = payload_bytes;
    h.payload_crc32       = 0;
    h.reserved32          = 0;
    h.previous_version_of = 0;
    h.reserved64          = 0;

    return h;
}

// -----------------------------------------------------------------------------
// Validation helpers
// -----------------------------------------------------------------------------
constexpr inline bool valid_magic(const DtxHeader& h) noexcept
{
    return h.magic[0] == 'D' &&
           h.magic[1] == 'T' &&
           h.magic[2] == 'X' &&
           h.magic[3] == '1';
}

constexpr inline bool valid_object_tag(const DtxObjectHeader& h) noexcept
{
    return h.tag[0] == 'O' &&
           h.tag[1] == 'B' &&
           h.tag[2] == 'J' &&
           h.tag[3] == '1';
}

constexpr inline bool version_supported(const DtxHeader& h) noexcept
{
    return h.version_major == kVersionMaj;
}

constexpr inline bool header_shape_valid(const DtxHeader& h) noexcept
{
    return h.header_bytes == kHeaderBytes &&
           h.object_align >= 1 &&
           h.first_object_offset >= kHeaderBytes &&
           h.append_offset >= h.first_object_offset;
}

constexpr inline std::uint64_t align_up(std::uint64_t n,
                                        std::uint64_t align = kObjectAlign) noexcept
{
    return (align == 0) ? n : ((n + align - 1) / align) * align;
}

constexpr inline std::uint64_t object_span_bytes(std::uint64_t payload_bytes) noexcept
{
    return align_up(sizeof(DtxObjectHeader) + payload_bytes, kObjectAlign);
}

// -----------------------------------------------------------------------------
// Reference-token helpers
// -----------------------------------------------------------------------------
inline constexpr char kHexDigits[] = "0123456789ABCDEF";

constexpr inline std::array<char, kRefTokenChars> encode_object_id_hex(std::uint64_t value) noexcept
{
    std::array<char, kRefTokenChars> out{};
    for (std::size_t i = 0; i < kRefTokenChars; ++i) {
        const std::size_t shift = (kRefTokenChars - 1 - i) * 4;
        out[i] = kHexDigits[(value >> shift) & 0x0F];
    }
    return out;
}

constexpr inline bool ref_token_is_blank(std::string_view sv) noexcept
{
    for (char c : sv) {
        if (c != ' ' && c != '\0') return false;
    }
    return true;
}

constexpr inline int hex_value(char c) noexcept
{
    return (c >= '0' && c <= '9') ? (c - '0') :
           (c >= 'A' && c <= 'F') ? (10 + (c - 'A')) :
           (c >= 'a' && c <= 'f') ? (10 + (c - 'a')) :
           -1;
}

constexpr inline bool decode_object_id_hex(std::string_view sv, std::uint64_t& out) noexcept
{
    if (sv.size() != kRefTokenChars) return false;

    std::uint64_t value = 0;
    for (char c : sv) {
        const int hv = hex_value(c);
        if (hv < 0) return false;
        value = (value << 4) | static_cast<std::uint64_t>(hv);
    }
    out = value;
    return true;
}

} // namespace dottalk::memo::dtx