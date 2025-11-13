// src/cnx/cnx_file.cpp
// Minimal CNX container implementation matching cnx/cnx.hpp (cnxfile::* API).
// Supports: open/create, read/flush header, read/write tagdir, add/drop tag.
// TableBind and validate_* are safe stubs for now.

#include "cnx/cnx.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace cnxfile {

// --------------------------- internal helpers ---------------------------

static std::string to_upper(std::string s) {
    for (auto &c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static void write_exact(std::fstream &io, const void *buf, std::streamsize n) {
    io.write(reinterpret_cast<const char *>(buf), n);
    if (!io) throw std::runtime_error("CNX: write failed");
}
static void read_exact(std::fstream &io, void *buf, std::streamsize n) {
    io.read(reinterpret_cast<char *>(buf), n);
    if (!io) throw std::runtime_error("CNX: read failed");
}

static std::uint64_t file_size(std::fstream &io) {
    auto cur = io.tellg();
    io.seekg(0, std::ios::end);
    auto end = io.tellg();
    io.seekg(cur);
    return static_cast<std::uint64_t>(end);
}

static void ensure_size(std::fstream &io, std::uint64_t size) {
    auto cur = io.tellp();
    io.seekp(0, std::ios::end);
    auto end = static_cast<std::uint64_t>(io.tellp());
    if (end < size) {
        std::vector<char> zeros(static_cast<size_t>(size - end), 0);
        io.write(zeros.data(), (std::streamsize)zeros.size());
    }
    io.seekp(cur);
}

// ------------------------------ handle ------------------------------

struct CNXHandle {
    std::fstream io;
    std::string  path;
    CNXHeader    hdr{};
    bool         hdr_dirty = false;
};

// Write header at offset 0 (no validation here).
static void store_header(CNXHandle *h) {
    h->io.seekp(0, std::ios::beg);
    write_exact(h->io, &h->hdr, (std::streamsize)sizeof(CNXHeader));
    h->io.flush();
    h->hdr_dirty = false;
}

// Read header from offset 0 and validate basic fields.
static bool load_header(CNXHandle *h) {
    h->io.seekg(0, std::ios::beg);
    CNXHeader tmp{};
    try {
        read_exact(h->io, &tmp, (std::streamsize)sizeof(CNXHeader));
    } catch (...) {
        return false;
    }
    if (tmp.magic != CNX_MAGIC || tmp.version != CNX_VERSION ||
        tmp.page_size == 0) {
        return false;
    }
    h->hdr = tmp;
    return true;
}

// Create a fresh file with default header and empty tagdir.
static void init_fresh_file(CNXHandle *h) {
    h->hdr = {};
    h->hdr.magic     = CNX_MAGIC;
    h->hdr.version   = CNX_VERSION;
    h->hdr.page_size = CNX_DEFAULT_PAGE_SIZE;
    h->hdr.flags     = 0;
    h->hdr.tag_count = 0;

    // Place tagdir immediately after header, aligned to page_size.
    const std::uint64_t hdr_end = sizeof(CNXHeader);
    const std::uint32_t ps = h->hdr.page_size;
    const std::uint64_t pad = (hdr_end % ps) ? (ps - (hdr_end % ps)) : 0;
    h->hdr.tagdir_offset = hdr_end + pad;

    // Write header and ensure file has at least one page.
    h->io.seekp(0, std::ios::beg);
    store_header(h);
    const std::uint64_t min_size = ((h->hdr.tagdir_offset + ps - 1) / ps) * ps;
    ensure_size(h->io, min_size);
    h->io.flush();
}

// Read all tag entries from tagdir into memory vector.
static bool read_tagdir_inner(CNXHandle *h, std::vector<TagInfo> &out) {
    out.clear();
    if (h->hdr.tag_count == 0) return true;

    const std::uint64_t off = h->hdr.tagdir_offset;
    h->io.seekg((std::streamoff)off, std::ios::beg);

    for (std::uint32_t i = 0; i < h->hdr.tag_count; ++i) {
        TagDirEntry e{};
        try { read_exact(h->io, &e, (std::streamsize)sizeof(e)); }
        catch (...) { return false; }

        // sanitize name to std::string (trim at first NUL)
        std::string name;
        for (char c : e.name) {
            if (c == '\0') break;
            name.push_back(c);
        }

        TagInfo ti{};
        ti.name         = name;
        ti.tag_id       = e.tag_id;
        ti.flags        = e.flags;
        ti.collation_id = e.collation_id;
        ti.expr_hash64  = e.expr_hash64;
        ti.for_hash64   = e.for_hash64;
        ti.root_page_off= e.root_page_off;
        ti.key_type     = e.key_type;
        ti.key_len      = e.key_len;
        ti.stats_rec    = e.stats_rec;
        ti.updated_ts   = e.updated_ts;
        out.push_back(ti);
    }
    return true;
}

// Write entire tagdir vector to file and update header.tag_count.
static bool write_tagdir_inner(CNXHandle *h, const std::vector<TagInfo> &tags) {
    const std::uint64_t off = h->hdr.tagdir_offset;
    h->io.seekp((std::streamoff)off, std::ios::beg);

    for (std::uint32_t i = 0; i < tags.size(); ++i) {
        TagDirEntry e{};
        std::string u = to_upper(tags[i].name);

        // clamp and NUL-terminate
        std::memset(e.name, 0, sizeof(e.name));
        const size_t copy_n = std::min(u.size(), sizeof(e.name) - 1);
        std::memcpy(e.name, u.data(), copy_n);

        e.tag_id        = (tags[i].tag_id != 0) ? tags[i].tag_id : (i + 1);
        e.flags         = tags[i].flags;
        e.collation_id  = tags[i].collation_id;
        e.expr_hash64   = tags[i].expr_hash64;
        e.for_hash64    = tags[i].for_hash64;
        e.root_page_off = tags[i].root_page_off;
        e.key_type      = tags[i].key_type;
        e.key_len       = tags[i].key_len;
        e.stats_rec     = tags[i].stats_rec;
        e.updated_ts    = tags[i].updated_ts;

        write_exact(h->io, &e, (std::streamsize)sizeof(e));
    }

    // Update header and pad to page boundary.
    h->hdr.tag_count = static_cast<std::uint32_t>(tags.size());
    store_header(h);

    const std::uint64_t end_off =
        h->hdr.tagdir_offset + (std::uint64_t)tags.size() * sizeof(TagDirEntry);
    const std::uint32_t ps = h->hdr.page_size;
    const std::uint64_t pad = (end_off % ps) ? (ps - (end_off % ps)) : 0;

    if (pad) {
        h->io.seekp((std::streamoff)end_off, std::ios::beg);
        std::vector<char> zeros((size_t)pad, 0);
        write_exact(h->io, zeros.data(), (std::streamsize)zeros.size());
    }
    h->io.flush();
    return true;
}

// ------------------------------ public API ------------------------------

bool open(const std::string &path, CNXHandle *&out) {
    out = nullptr;

    // Try open existing read/write; if missing, create it.
    const bool existed = fs::exists(path);
    std::fstream io(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!io) {
        // create new
        std::fstream create(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!create) return false;
        create.close();
        io.open(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!io) return false;
    }

    std::unique_ptr<CNXHandle> h(new CNXHandle{});
    h->io   = std::move(io);
    h->path = path;

    if (!existed || file_size(h->io) < sizeof(CNXHeader)) {
        init_fresh_file(h.get());
    } else {
        if (!load_header(h.get())) {
            // Re-init corrupt/mismatched header as fresh file to keep tools usable.
            h->io.close();
            std::fstream recreate(path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!recreate) return false;
            recreate.close();
            h->io.open(path, std::ios::in | std::ios::out | std::ios::binary);
            if (!h->io) return false;
            init_fresh_file(h.get());
        }
    }

    out = h.release();
    return true;
}

void close(CNXHandle *&h) {
    if (!h) return;
    if (h->hdr_dirty) {
        try { store_header(h); } catch (...) {}
    }
    h->io.flush();
    h->io.close();
    delete h;
    h = nullptr;
}

bool read_header(CNXHandle *h, CNXHeader &hdr) {
    if (!h) return false;
    // Refresh from disk to reflect any external edits.
    if (!load_header(h)) return false;
    hdr = h->hdr;
    return true;
}

bool set_dirty(CNXHandle *h, bool dirty) {
    if (!h) return false;
    if (dirty) h->hdr.flags |= CNX_HDRF_DIRTY;
    else       h->hdr.flags &= ~CNX_HDRF_DIRTY;
    h->hdr_dirty = true;
    return true;
}

bool flush_header(CNXHandle *h, const CNXHeader &hdr) {
    if (!h) return false;
    h->hdr = hdr;
    store_header(h);
    return true;
}

std::optional<uint32_t> page_size(CNXHandle *h) {
    if (!h) return std::nullopt;
    return h->hdr.page_size;
}

// ---------- TableBind (page 1) ----------
// For now we don’t persist a real TableBind page; keep no-ops that succeed.

bool read_table_bind(CNXHandle *h, TableBind &out) {
    if (!h) return false;
    std::memset(&out, 0, sizeof(out));
    return true;
}
bool write_table_bind(CNXHandle *h, const TableBind &in) {
    (void)h; (void)in;
    return true;
}

// ---------- Tag directory ----------

bool read_tagdir(CNXHandle *h, std::vector<TagInfo> &out) {
    if (!h) return false;
    try { return read_tagdir_inner(h, out); }
    catch (...) { return false; }
}

bool write_tagdir(CNXHandle *h, const std::vector<TagInfo> &tags) {
    if (!h) return false;
    try { return write_tagdir_inner(h, tags); }
    catch (...) { return false; }
}

bool add_tag(CNXHandle *h, const std::string &tag_name_upper) {
    if (!h) return false;

    std::vector<TagInfo> tags;
    if (!read_tagdir_inner(h, tags)) return false;

    const std::string up = to_upper(tag_name_upper);
    auto it = std::find_if(tags.begin(), tags.end(),
        [&](const TagInfo &ti){ return to_upper(ti.name) == up; });
    if (it != tags.end()) return false; // duplicate

    TagInfo ti{};
    ti.name       = up;
    ti.tag_id     = tags.empty() ? 1u : (tags.back().tag_id + 1u);
    ti.updated_ts = (std::uint64_t)std::chrono::system_clock::to_time_t(
                        std::chrono::system_clock::now());

    tags.push_back(ti);
    if (!write_tagdir_inner(h, tags)) return false;

    // Mark header clean after successful structural update.
    set_dirty(h, false);
    store_header(h);
    return true;
}

bool drop_tag(CNXHandle *h, const std::string &tag_name_upper) {
    if (!h) return false;

    std::vector<TagInfo> tags;
    if (!read_tagdir_inner(h, tags)) return false;

    const std::string up = to_upper(tag_name_upper);
    auto it = std::remove_if(tags.begin(), tags.end(),
        [&](const TagInfo &ti){ return to_upper(ti.name) == up; });

    if (it == tags.end()) return false; // not found
    tags.erase(it, tags.end());
    if (!write_tagdir_inner(h, tags)) return false;

    set_dirty(h, false);
    store_header(h);
    return true;
}

// ---------- Attach validation (stub) ----------

BindCheck validate_table_bind(const TableBind &bind,
                              const TableProbe &probe,
                              BindPolicy policy) {
    (void)bind; (void)probe; (void)policy;
    BindCheck bc{};
    bc.ok = true;
    bc.warn = false;
    bc.message.clear();
    return bc;
}

} // namespace cnxfile
