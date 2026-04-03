#pragma once
// include/cnx/cdx.hpp
// Minimal CDX container: header I/O, tag directory read/write, add/drop tag.
// NOTE: This does not build keys; REBUILD/COMPACT are separate concerns.

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <cstddef>   // std::size_t

namespace cdxfile {

static constexpr uint32_t CDX_MAGIC              = 0x31584443; // "CDX1" little-end
static constexpr uint32_t CDX_VERSION            = 1;
static constexpr uint32_t CDX_DEFAULT_PAGE_SIZE  = 4096;

enum : uint32_t { CDX_HDRF_DIRTY = 0x0001 };

struct CDXHeader {
    uint32_t magic      = CDX_MAGIC;
    uint32_t version    = CDX_VERSION;
    uint32_t page_size  = CDX_DEFAULT_PAGE_SIZE;
    uint32_t flags      = 0;

    uint64_t tagdir_offset = 0;
    uint32_t tag_count     = 0;
    uint32_t reserved0     = 0;

    uint64_t reserved1 = 0;
    uint64_t reserved2 = 0;
    uint64_t reserved3 = 0;
};

struct TagDirEntry {
    char     name[32]      = {0};
    uint32_t tag_id        = 0;
    uint32_t flags         = 0;
    uint32_t collation_id  = 0;
    uint64_t expr_hash64   = 0;
    uint64_t for_hash64    = 0;
    uint64_t root_page_off = 0;
    uint32_t key_type      = 0;
    uint32_t key_len       = 0;
    uint64_t stats_rec     = 0;
    uint64_t updated_ts    = 0;
};

// NOTE: Ordering matters for aggregate initialization.
// We keep tag_id first so call sites can do: TagInfo{1, "LNAME", ...}.
struct TagInfo {
    uint32_t    tag_id        = 0;
    std::string name;                 // uppercased tag name
    uint32_t    flags         = 0;
    uint32_t    collation_id  = 0;
    uint64_t    expr_hash64   = 0;
    uint64_t    for_hash64    = 0;
    uint64_t    root_page_off = 0;
    uint32_t    key_type      = 0;
    uint32_t    key_len       = 0;
    uint64_t    stats_rec     = 0;
    uint64_t    updated_ts    = 0;
};

struct TableBind {
    uint8_t  table_uuid[16]{};
    uint64_t schema_hash64 = 0;
    uint32_t record_len    = 0;
    uint32_t field_count   = 0;
    char     table_basename[40]{};
    uint64_t build_dbftime = 0;
    uint8_t  _pad[64]{};
};

struct TableProbe {
    uint8_t  table_uuid[16]{};
    uint64_t schema_hash64 = 0;
    uint32_t record_len    = 0;
    uint32_t field_count   = 0;
    std::string table_basename_upper;
    uint64_t dbf_mtime = 0;
};

enum class BindPolicy { STRICT, WARN, LOOSE };

struct BindCheck {
    bool ok = false;
    bool warn = false;
    std::string message;
};

struct CDXHandle; // opaque

// ---------- Open/close/header ----------
bool open(const std::string& path, CDXHandle*& out);
void close(CDXHandle*& h);
bool read_header(CDXHandle* h, CDXHeader& hdr);
bool set_dirty(CDXHandle* h, bool dirty);
bool flush_header(CDXHandle* h, const CDXHeader& hdr);
std::optional<uint32_t> page_size(CDXHandle* h);

// ---------- TableBind (page 1) ----------
bool read_table_bind(CDXHandle* h, TableBind& out);
bool write_table_bind(CDXHandle* h, const TableBind& in);

// ---------- Tag directory ----------
bool read_tagdir(CDXHandle* h, std::vector<TagInfo>& out);
bool write_tagdir(CDXHandle* h, const std::vector<TagInfo>& tags);

bool add_tag(CDXHandle* h, const std::string& tag_name_upper);
bool drop_tag(CDXHandle* h, const std::string& tag_name_upper);

// ---------- Attach validation ----------
BindCheck validate_table_bind(const TableBind& bind,
                              const TableProbe& probe,
                              BindPolicy policy);

// ---------- Minimal raw I/O helpers (for RUN blocks, future B-tree pages) ----------
bool append_bytes(CDXHandle* h, const void* data, std::size_t len, std::uint64_t& out_off);
bool read_at(CDXHandle* h, std::uint64_t off, void* buf, std::size_t len);
bool write_at(CDXHandle* h, std::uint64_t off, const void* buf, std::size_t len);

} // namespace cdxfile