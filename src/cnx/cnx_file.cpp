// src/cnx/cnx_file.cpp
// Minimal CNX container implementation matching include/cnx/cnx.hpp (cnxfile::* API).
// Supports: open/create, read/flush header, read/write tagdir, add/drop tag.
// Adds: append_bytes / read_at / write_at for RUN blocks (future b-tree pages).

#include "cnx/cnx.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cnxfile {

// IMPORTANT: This must be cnxfile::CNXHandle (NOT an anonymous-namespace type)
struct CNXHandle {
    std::fstream io;
    std::string  path;
    CNXHeader    hdr{};
    bool         hdr_dirty = false;
};

namespace {

static void read_exact(std::istream& is, void* buf, std::streamsize n)
{
    is.read(reinterpret_cast<char*>(buf), n);
    if (is.gcount() != n) throw std::runtime_error("read_exact failed");
}

static void write_exact(std::ostream& os, const void* buf, std::streamsize n)
{
    os.write(reinterpret_cast<const char*>(buf), n);
    if (!os) throw std::runtime_error("write_exact failed");
}

static std::uint64_t align_up(std::uint64_t v, std::uint64_t a)
{
    if (a == 0) return v;
    const std::uint64_t r = v % a;
    return r ? (v + (a - r)) : v;
}

static std::uint64_t now_unix()
{
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

static void store_header(CNXHandle* h)
{
    h->io.seekp(0, std::ios::beg);
    write_exact(h->io, &h->hdr, (std::streamsize)sizeof(CNXHeader));
    h->io.flush();
    h->hdr_dirty = false;
}

static bool load_header(CNXHandle* h)
{
    h->io.seekg(0, std::ios::beg);
    CNXHeader tmp{};
    try { read_exact(h->io, &tmp, (std::streamsize)sizeof(CNXHeader)); }
    catch (...) { return false; }

    if (tmp.magic != CNX_MAGIC || tmp.version != CNX_VERSION || tmp.page_size == 0)
        return false;

    h->hdr = tmp;
    return true;
}

static void init_fresh_file(CNXHandle* h)
{
    h->hdr = {};
    h->hdr.magic     = CNX_MAGIC;
    h->hdr.version   = CNX_VERSION;
    h->hdr.page_size = CNX_DEFAULT_PAGE_SIZE;
    h->hdr.flags     = 0;
    h->hdr.tag_count = 0;

    const std::uint64_t hdr_end = (std::uint64_t)sizeof(CNXHeader);
    h->hdr.tagdir_offset = align_up(hdr_end, h->hdr.page_size);

    // Ensure file is at least tagdir_offset bytes.
    h->io.seekp((std::streamoff)h->hdr.tagdir_offset, std::ios::beg);
    h->io.flush();

    store_header(h);
}

static void fill_entry_name(TagDirEntry& e, const std::string& upname)
{
    std::memset(e.name, 0, sizeof(e.name));
    const size_t n = std::min(upname.size(), sizeof(e.name) - 1);
    std::memcpy(e.name, upname.data(), n);
}

static bool read_tagdir_inner(CNXHandle* h, std::vector<TagInfo>& out)
{
    out.clear();
    if (!h) return false;

    if (h->hdr.tag_count == 0) return true;

    h->io.seekg((std::streamoff)h->hdr.tagdir_offset, std::ios::beg);

    for (std::uint32_t i = 0; i < h->hdr.tag_count; ++i) {
        TagDirEntry e{};
        try { read_exact(h->io, &e, (std::streamsize)sizeof(e)); }
        catch (...) { return false; }

        std::string name;
        for (char c : e.name) {
            if (c == '\0') break;
            name.push_back(c);
        }
        while (!name.empty() && name.back() == ' ') name.pop_back();

        TagInfo t{};
        t.name         = name;
        t.tag_id       = e.tag_id;
        t.flags        = e.flags;
        t.collation_id = e.collation_id;
        t.expr_hash64  = e.expr_hash64;
        t.for_hash64   = e.for_hash64;
        t.root_page_off= e.root_page_off;
        t.key_type     = e.key_type;
        t.key_len      = e.key_len;
        t.stats_rec    = e.stats_rec;
        t.updated_ts   = e.updated_ts;

        out.push_back(std::move(t));
    }
    return true;
}

static bool write_tagdir_inner(CNXHandle* h, const std::vector<TagInfo>& tags)
{
    if (!h) return false;

    h->io.seekp((std::streamoff)h->hdr.tagdir_offset, std::ios::beg);

    for (std::uint32_t i = 0; i < (std::uint32_t)tags.size(); ++i) {
        TagDirEntry e{};
        fill_entry_name(e, tags[i].name);
        e.tag_id        = tags[i].tag_id;
        e.flags         = tags[i].flags;
        e.collation_id  = tags[i].collation_id;
        e.expr_hash64   = tags[i].expr_hash64;
        e.for_hash64    = tags[i].for_hash64;
        e.root_page_off = tags[i].root_page_off;
        e.key_type      = tags[i].key_type;
        e.key_len       = tags[i].key_len;
        e.stats_rec     = tags[i].stats_rec;
        e.updated_ts    = tags[i].updated_ts;

        try { write_exact(h->io, &e, (std::streamsize)sizeof(e)); }
        catch (...) { return false; }
    }

    h->hdr.tag_count = (std::uint32_t)tags.size();
    h->hdr.flags &= ~CNX_HDRF_DIRTY;

    try { store_header(h); } catch (...) {}
    return true;
}

static bool tag_exists(const std::vector<TagInfo>& tags, const std::string& up)
{
    for (const auto& t : tags) if (t.name == up) return true;
    return false;
}

} // anonymous namespace

// ---------- Open/close/header ----------

bool open(const std::string& path, CNXHandle*& out)
{
    out = nullptr;

    auto h = std::make_unique<CNXHandle>();
    h->path = path;

    h->io.open(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!h->io.is_open()) {
        // create
        h->io.clear();
        h->io.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!h->io.is_open()) return false;
        h->io.close();

        h->io.open(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!h->io.is_open()) return false;

        init_fresh_file(h.get());
    } else {
        if (!load_header(h.get())) return false;
    }

    out = h.release();
    return true;
}

void close(CNXHandle*& h)
{
    if (!h) return;
    try { if (h->hdr_dirty) store_header(h); } catch (...) {}
    try { h->io.flush(); } catch (...) {}
    try { h->io.close(); } catch (...) {}
    delete h;
    h = nullptr;
}

bool read_header(CNXHandle* h, CNXHeader& hdr)
{
    if (!h) return false;
    hdr = h->hdr;
    return true;
}

bool set_dirty(CNXHandle* h, bool dirty)
{
    if (!h) return false;
    h->hdr_dirty = dirty;
    if (dirty) h->hdr.flags |= CNX_HDRF_DIRTY;
    else       h->hdr.flags &= ~CNX_HDRF_DIRTY;
    return true;
}

bool flush_header(CNXHandle* h, const CNXHeader& hdr)
{
    if (!h) return false;
    h->hdr = hdr;
    try { store_header(h); }
    catch (...) { return false; }
    return true;
}

std::optional<uint32_t> page_size(CNXHandle* h)
{
    if (!h) return std::nullopt;
    return h->hdr.page_size;
}

// ---------- TableBind (stubs) ----------

bool read_table_bind(CNXHandle* h, TableBind& out)
{
    (void)h;
    std::memset(&out, 0, sizeof(out));
    return true;
}

bool write_table_bind(CNXHandle* h, const TableBind& in)
{
    (void)h; (void)in;
    return true;
}

// ---------- Tag directory ----------

bool read_tagdir(CNXHandle* h, std::vector<TagInfo>& out)
{
    try { return read_tagdir_inner(h, out); }
    catch (...) { return false; }
}

bool write_tagdir(CNXHandle* h, const std::vector<TagInfo>& tags)
{
    try { return write_tagdir_inner(h, tags); }
    catch (...) { return false; }
}

bool add_tag(CNXHandle* h, const std::string& tag_name_upper)
{
    if (!h) return false;

    std::vector<TagInfo> tags;
    if (!read_tagdir_inner(h, tags)) return false;

    if (tag_exists(tags, tag_name_upper)) return false;

    TagInfo t{};
    t.name = tag_name_upper;
    t.tag_id = (std::uint32_t)(tags.size() + 1);
    t.updated_ts = now_unix();

    tags.push_back(std::move(t));
    return write_tagdir_inner(h, tags);
}

bool drop_tag(CNXHandle* h, const std::string& tag_name_upper)
{
    if (!h) return false;

    std::vector<TagInfo> tags;
    if (!read_tagdir_inner(h, tags)) return false;

    auto it = std::remove_if(tags.begin(), tags.end(),
        [&](const TagInfo& t){ return t.name == tag_name_upper; });

    if (it == tags.end()) return false;

    tags.erase(it, tags.end());

    for (std::uint32_t i = 0; i < (std::uint32_t)tags.size(); ++i)
        tags[i].tag_id = i + 1;

    return write_tagdir_inner(h, tags);
}

// ---------- Attach validation (stub) ----------

BindCheck validate_table_bind(const TableBind& bind,
                              const TableProbe& probe,
                              BindPolicy policy)
{
    (void)bind; (void)probe; (void)policy;
    BindCheck bc{};
    bc.ok = true;
    bc.warn = false;
    return bc;
}

// ---------- Raw I/O helpers (REAL) ----------

bool append_bytes(CNXHandle* h, const void* data, std::size_t len, std::uint64_t& out_off)
{
    if (!h) return false;
    try {
        h->io.seekp(0, std::ios::end);
        const std::streampos pos = h->io.tellp();
        if (pos < 0) return false;
        out_off = static_cast<std::uint64_t>(pos);

        write_exact(h->io, data, static_cast<std::streamsize>(len));
        h->io.flush();
        return true;
    }
    catch (...) { return false; }
}

bool read_at(CNXHandle* h, std::uint64_t off, void* buf, std::size_t len)
{
    if (!h) return false;
    try {
        h->io.seekg(static_cast<std::streamoff>(off), std::ios::beg);
        read_exact(h->io, buf, static_cast<std::streamsize>(len));
        return true;
    }
    catch (...) { return false; }
}

bool write_at(CNXHandle* h, std::uint64_t off, const void* buf, std::size_t len)
{
    if (!h) return false;
    try {
        h->io.seekp(static_cast<std::streamoff>(off), std::ios::beg);
        write_exact(h->io, buf, static_cast<std::streamsize>(len));
        h->io.flush();
        return true;
    }
    catch (...) { return false; }
}

} // namespace cnxfile
