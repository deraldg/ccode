#pragma once
// Minimal CNX container: header I/O, tag directory read/write, add/drop tag.
// NOTE: This does not build keys; REBUILD/COMPACT are separate concerns.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <optional>

namespace cnxfile {

static constexpr uint32_t CNX_MAGIC              = 0x31584e43; // "CNX1" little-end
static constexpr uint32_t CNX_VERSION            = 1;
static constexpr uint32_t CNX_DEFAULT_PAGE_SIZE  = 4096;

enum : uint32_t { CNX_HDRF_DIRTY = 0x0001 };

struct CNXHeader {
    uint32_t magic      = CNX_MAGIC;
    uint32_t version    = CNX_VERSION;
    uint32_t page_size  = CNX_DEFAULT_PAGE_SIZE;
    uint32_t flags      = 0;

    uint64_t tagdir_offset = 0;   // absolute file offset of first TagDirEntry
    uint32_t tag_count     = 0;   // number of entries
    uint32_t reserved0     = 0;

    uint64_t reserved1 = 0;
    uint64_t reserved2 = 0;
    uint64_t reserved3 = 0;
};

// On-disk fixed entry
struct TagDirEntry {
    char     name[32]      = {0}; // upper-case, NUL-terminated
    uint32_t tag_id        = 0;   // sequential id
    uint32_t flags         = 0;   // 0 for now
    uint32_t collation_id  = 0;   // 0 = binary
    uint64_t expr_hash64   = 0;   // future use
    uint64_t for_hash64    = 0;   // future use
    uint64_t root_page_off = 0;   // future btree root (0=not built)
    uint32_t key_type      = 0;   // 0=string, 1=number, etc. (future)
    uint32_t key_len       = 0;   // future (0=var)
    uint64_t stats_rec     = 0;   // records at build time (informational)
    uint64_t updated_ts    = 0;   // unix ts (optional)
};

// In-memory convenience
struct TagInfo {
    std::string name;
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

struct TableBind {
    // page 1 structure; optional — left opaque for now
    uint8_t  table_uuid[16]{};
    uint64_t schema_hash64 = 0;
    uint32_t record_len    = 0;
    uint32_t field_count   = 0;
    char     table_basename[40]{};
    uint64_t build_dbftime = 0; // optional timestamp of DBF at build
    uint8_t  _pad[64]{};
};

struct TableProbe {
    // in-memory probe of a DBF (opaque to cnx file)
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

struct CNXHandle; // opaque

// ---------- Open/close/header ----------
bool open(const std::string& path, CNXHandle*& out);
void close(CNXHandle*& h);
bool read_header(CNXHandle* h, CNXHeader& hdr);
bool set_dirty(CNXHandle* h, bool dirty);
bool flush_header(CNXHandle* h, const CNXHeader& hdr);
std::optional<uint32_t> page_size(CNXHandle* h);

// ---------- TableBind (page 1) ----------
bool read_table_bind(CNXHandle* h, TableBind& out);
bool write_table_bind(CNXHandle* h, const TableBind& in);

// ---------- Tag directory ----------
bool read_tagdir(CNXHandle* h, std::vector<TagInfo>& out);

// New: write/replace entire tagdir vector to file and update header
bool write_tagdir(CNXHandle* h, const std::vector<TagInfo>& tags);

// New: convenience helpers
bool add_tag(CNXHandle* h, const std::string& tag_name_upper);
bool drop_tag(CNXHandle* h, const std::string& tag_name_upper);

// ---------- Attach validation ----------
BindCheck validate_table_bind(const TableBind& bind,
                              const TableProbe& probe,
                              BindPolicy policy);

} // namespace cnxfile
