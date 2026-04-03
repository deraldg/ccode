// src/xindex/cnx_backend.cpp

#include "xindex/cnx_backend.hpp"
#include "xindex/dbarea_adapt.hpp"
#include "xbase.hpp"

#include "cnx/cnx.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <vector>

namespace xindex {

namespace {

// simple uppercase copy
inline std::string up(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (unsigned char c : s)
        o.push_back(static_cast<char>(std::toupper(c)));
    return o;
}

// resolve the DBF field type for the given tag (by uppercased name)
inline char resolve_tag_type(const xbase::DbArea& area,
                             const std::string& tag_upper) {
    const auto& fdefs = area.fields();
    for (const auto& f : fdefs) {
        std::string nm = f.name;
        for (auto& ch : nm)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (nm == tag_upper)
            return f.type;   // 'C', 'N', 'D', 'L', etc.
    }
    return 'C';
}

// ---- RUN1 persisted sorted-run format ----
static constexpr std::uint32_t RUN1_MAGIC = 0x314E5552; // "RUN1" little-end

#pragma pack(push,1)
struct Run1Header {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t tag_id;
    std::uint32_t key_type;      // 0=char(bytes), 1=date(bytes)
    std::uint32_t key_len;       // 0=var
    std::uint64_t record_count;
    std::uint64_t data_bytes;
    std::uint8_t  reserved[32];
};
#pragma pack(pop)

} // namespace

// ------------------------- Cursor impl -------------------------
struct CnxBackend::TreeCursor : public Cursor {
    using Iter = Multimap::const_iterator;

    Iter begin;
    Iter it;
    Iter end;
    bool started{false};

    TreeCursor(Iter b, Iter start, Iter e) : begin(b), it(start), end(e) {}

    bool first(Key& outKey, RecNo& outRec) override {
        started = true;
        it = begin;
        if (it == end) return false;
        outKey = it->first;
        outRec = it->second;
        return true;
    }

    bool next(Key& outKey, RecNo& outRec) override {
        if (!started) return first(outKey, outRec);
        if (it == end) return false;
        ++it;
        if (it == end) return false;
        outKey = it->first;
        outRec = it->second;
        return true;
    }

    bool last(Key& outKey, RecNo& outRec) override {
        started = true;
        if (begin == end) return false;
        it = end;
        --it;
        outKey = it->first;
        outRec = it->second;
        return true;
    }

    bool prev(Key& outKey, RecNo& outRec) override {
        if (!started) return last(outKey, outRec);
        if (begin == end) return false;

        if (it == end) {
            // If we've already walked past the end, prev() should land on the last element.
            it = end;
            --it;
        } else {
            if (it == begin) return false;
            --it;
        }

        outKey = it->first;
        outRec = it->second;
        return true;
    }
};

// ------------------------- Backend -------------------------
CnxBackend::CnxBackend(xbase::DbArea& area,
                       std::string cnx_path,
                       std::string tag_upper)
    : area_(area),
      cnx_path_(std::move(cnx_path)),
      tag_upper_(up(tag_upper))
{
    tag_type_ = resolve_tag_type(area_, tag_upper_);
}

bool CnxBackend::open(const std::string& path) {
    cnx_path_ = path;
    load_from_cnx(); // now real for RUN1
    return true;
}

void CnxBackend::close() {
    save_to_cnx();   // now real for RUN1
    tree_.clear();
}

void CnxBackend::rebuild() {
    tree_.clear();
    const int n = xindex::db_record_count(area_);
    for (int rn = 1; rn <= n; ++rn) {
        if (xindex::db_is_deleted(area_, rn)) continue;
        Key k = make_key_from_record(static_cast<std::uint32_t>(rn));
        tree_.emplace(std::move(k), static_cast<RecNo>(rn));
    }
    stale_ = false;

    // IMPORTANT: persist immediately so REBUILD writes back to CNX
    // (even if callers never call close()).
    save_to_cnx();
}

void CnxBackend::upsert(const Key& key, RecNo rec) {
    for (auto it = tree_.begin(); it != tree_.end(); ) {
        if (it->second == rec) it = tree_.erase(it);
        else ++it;
    }
    tree_.emplace(key, rec);
    stale_ = false;
}

void CnxBackend::erase(const Key& key, RecNo rec) {
    auto range = tree_.equal_range(key);
    for (auto it = range.first; it != range.second; ) {
        if (it->second == rec) { it = tree_.erase(it); break; }
        else ++it;
    }
    stale_ = false;
}

std::unique_ptr<Cursor> CnxBackend::seek(const Key& key) const {
    auto it = tree_.lower_bound(key);
    return std::unique_ptr<Cursor>(new TreeCursor(tree_.begin(), it, tree_.end()));
}

std::unique_ptr<Cursor> CnxBackend::scan(const Key& low, const Key& high) const {
    auto a = tree_.lower_bound(low);
    auto b = tree_.upper_bound(high);
    return std::unique_ptr<Cursor>(new TreeCursor(a, a, b));
}

Key CnxBackend::make_key_from_string(const std::string& s) {
    return codec::encodeChar(
        up(s),
        s.empty() ? 1 : static_cast<int>(s.size()),
        /*upper*/true);
}

Key CnxBackend::make_key_from_record(std::uint32_t rec) const {
    const std::string raw =
        xindex::db_get_string(area_, static_cast<int>(rec), tag_upper_);

    if (tag_type_ == 'D')
        return codec::encodeDateYYYYMMDD(raw);

    return make_key_from_string(raw);
}

void CnxBackend::save_to_cnx()
{
    // If empty, don't mark built. (Leave root_page_off as-is.)
    if (tree_.empty()) return;

    cnxfile::CNXHandle* h = nullptr;
    if (!cnxfile::open(cnx_path_, h)) return;

    std::vector<cnxfile::TagInfo> tags;
    if (!cnxfile::read_tagdir(h, tags)) { cnxfile::close(h); return; }

    auto itTag = std::find_if(tags.begin(), tags.end(),
        [&](const cnxfile::TagInfo& t){ return t.name == tag_upper_; });

    if (itTag == tags.end()) { cnxfile::close(h); return; }

    Run1Header rh{};
    rh.magic = RUN1_MAGIC;
    rh.version = 1;
    rh.tag_id = itTag->tag_id;
    rh.key_type = (tag_type_ == 'D') ? 1u : 0u;
    rh.key_len = 0;
    rh.record_count = static_cast<std::uint64_t>(tree_.size());

    std::uint64_t bytes = 0;
    for (const auto& kv : tree_) {
        bytes += sizeof(std::uint32_t); // recno
        bytes += sizeof(std::uint32_t); // keylen
        bytes += static_cast<std::uint64_t>(kv.first.size()); // key bytes
    }
    rh.data_bytes = bytes;

    std::uint64_t run_off = 0;
    if (!cnxfile::append_bytes(h, &rh, sizeof(rh), run_off)) {
        cnxfile::close(h);
        return;
    }

    // Append entries
    for (const auto& kv : tree_) {
        const std::uint32_t rec = static_cast<std::uint32_t>(kv.second);
        const std::uint32_t len = static_cast<std::uint32_t>(kv.first.size());

        std::uint64_t tmp_off = 0;
        if (!cnxfile::append_bytes(h, &rec, sizeof(rec), tmp_off)) { cnxfile::close(h); return; }
        if (!cnxfile::append_bytes(h, &len, sizeof(len), tmp_off)) { cnxfile::close(h); return; }
        if (len) {
            if (!cnxfile::append_bytes(h, kv.first.data(), len, tmp_off)) { cnxfile::close(h); return; }
        }
    }

    // Update tag directory metadata
    itTag->root_page_off = run_off;
    itTag->stats_rec     = rh.record_count;
    itTag->updated_ts    = static_cast<std::uint64_t>(std::time(nullptr));

    (void)cnxfile::write_tagdir(h, tags);
    cnxfile::close(h);
}

void CnxBackend::load_from_cnx()
{
    tree_.clear();

    cnxfile::CNXHandle* h = nullptr;
    if (!cnxfile::open(cnx_path_, h)) return;

    std::vector<cnxfile::TagInfo> tags;
    if (!cnxfile::read_tagdir(h, tags)) { cnxfile::close(h); return; }

    auto itTag = std::find_if(tags.begin(), tags.end(),
        [&](const cnxfile::TagInfo& t){ return t.name == tag_upper_; });

    if (itTag == tags.end() || itTag->root_page_off == 0) {
        cnxfile::close(h);
        return;
    }

    Run1Header rh{};
    if (!cnxfile::read_at(h, itTag->root_page_off, &rh, sizeof(rh))) {
        cnxfile::close(h);
        return;
    }
    if (rh.magic != RUN1_MAGIC || rh.version != 1) {
        cnxfile::close(h);
        return;
    }

    std::uint64_t off = itTag->root_page_off + sizeof(Run1Header);

    for (std::uint64_t i = 0; i < rh.record_count; ++i) {
        std::uint32_t rec = 0;
        std::uint32_t len = 0;

        if (!cnxfile::read_at(h, off, &rec, sizeof(rec))) { cnxfile::close(h); return; }
        off += sizeof(rec);

        if (!cnxfile::read_at(h, off, &len, sizeof(len))) { cnxfile::close(h); return; }
        off += sizeof(len);

        Key k;
        k.resize(len);
        if (len) {
            if (!cnxfile::read_at(h, off, k.data(), len)) { cnxfile::close(h); return; }
            off += len;
        }

        tree_.emplace(std::move(k), static_cast<RecNo>(rec));
    }

    stale_ = false;
    cnxfile::close(h);
}

} // namespace xindex