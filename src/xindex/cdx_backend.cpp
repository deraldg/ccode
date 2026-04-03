// src/xindex/cdx_backend.cpp
#include "xindex/cdx_backend.hpp"

#include "xbase.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace xindex {

#if !XINDEX_HAVE_LMDB

CdxBackend::CdxBackend(xbase::DbArea& area, std::string cdx_container_path)
    : area_(area), cdx_path_(std::move(cdx_container_path)) {}
CdxBackend::~CdxBackend() = default;

bool CdxBackend::open(const std::string&) { return false; }
void CdxBackend::close() {}
void CdxBackend::rebuild() {}
void CdxBackend::upsert(const Key&, RecNo) {}
void CdxBackend::erase(const Key&, RecNo) {}
std::unique_ptr<Cursor> CdxBackend::seek(const Key&) const { return {}; }
std::unique_ptr<Cursor> CdxBackend::scan(const Key&, const Key&) const { return {}; }
void CdxBackend::setTag(const std::string&) {}
bool CdxBackend::seekRecnoUserKey(const std::string&, std::uint32_t&, std::string& out_err) const {
    out_err = "LMDB not available in this build";
    return false;
}

#else

namespace {

inline int cmp_lex(const void* a, size_t an, const void* b, size_t bn) {
    const auto n = std::min(an, bn);
    const int r = std::memcmp(a, b, n);
    if (r != 0) return r;
    if (an < bn) return -1;
    if (an > bn) return 1;
    return 0;
}

inline bool decode_u64_le_local(const void* p, size_t n, std::uint64_t& out) {
    if (n < 8) return false;
    const auto* b = static_cast<const unsigned char*>(p);
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<std::uint64_t>(b[i]) << (8 * i));
    out = v;
    return true;
}

inline void encode_u64_le_local(std::uint64_t v, unsigned char out8[8]) {
    for (int i = 0; i < 8; ++i) out8[i] = static_cast<unsigned char>((v >> (8 * i)) & 0xFFu);
}

inline void key_from_bytes(Key& out, const void* p, size_t n) {
    out.clear();
    const auto* bp = static_cast<const std::uint8_t*>(p);
    out.insert(out.end(), bp, bp + n);
}

inline bool within_bounds_raw(const MDB_val& k,
                              const std::vector<std::uint8_t>& low,
                              const std::vector<std::uint8_t>& high,
                              bool has_low,
                              bool has_high) {
    if (has_low) {
        if (cmp_lex(k.mv_data, k.mv_size, low.data(), low.size()) < 0) return false;
    }
    if (has_high) {
        if (cmp_lex(k.mv_data, k.mv_size, high.data(), high.size()) > 0) return false;
    }
    return true;
}

inline std::vector<std::uint8_t> make_storage_key(bool composite,
                                                  const Key& key,
                                                  RecNo rec)
{
    std::vector<std::uint8_t> out;
    out.reserve(key.size() + (composite ? 8u : 0u));
    out.insert(out.end(), key.begin(), key.end());

    if (composite) {
        unsigned char rec8[8]{};
        encode_u64_le_local(static_cast<std::uint64_t>(rec), rec8);
        out.insert(out.end(), rec8, rec8 + 8);
    }

    return out;
}

inline std::vector<std::uint8_t> make_storage_val(RecNo rec)
{
    std::vector<std::uint8_t> out(8);
    encode_u64_le_local(static_cast<std::uint64_t>(rec), out.data());
    return out;
}

} // namespace

void CdxBackend::throw_on_mdb_err_(int rc, const char* what) {
    if (rc == MDB_SUCCESS) return;
    throw std::runtime_error(std::string(what) + ": " + mdb_strerror(rc));
}

std::string CdxBackend::trim_copy_(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string CdxBackend::upper_copy_(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

void CdxBackend::rtrim_lmdb_key_(std::string& s) {
    while (!s.empty() && (s.back() == '\0' || s.back() == ' ')) s.pop_back();
}

std::string CdxBackend::norm_key_(std::string s) {
    s = trim_copy_(s);
    s = upper_copy_(s);
    rtrim_lmdb_key_(s);
    return s;
}

void CdxBackend::encode_u64_le_(std::uint64_t v, unsigned char out8[8]) {
    encode_u64_le_local(v, out8);
}

bool CdxBackend::decode_u64_le_(const void* p, size_t n, std::uint64_t& out) {
    return decode_u64_le_local(p, n, out);
}

std::string CdxBackend::bytes_to_string_(const MDB_val& v) {
    if (!v.mv_data || v.mv_size == 0) return {};
    return std::string(static_cast<const char*>(v.mv_data),
                       static_cast<const char*>(v.mv_data) + v.mv_size);
}

CdxBackend::CdxBackend(xbase::DbArea& area, std::string cdx_container_path)
    : area_(area), cdx_path_(std::move(cdx_container_path)) {}

CdxBackend::~CdxBackend() { close(); }

bool CdxBackend::open_env_dir_(const std::string& env_dir, std::string* err) {
    close_env_();

    env_dir_ = env_dir;
    if (env_dir_.empty()) {
        if (err) *err = "env dir is empty";
        return false;
    }

    int rc = mdb_env_create(&env_);
    if (rc != MDB_SUCCESS) {
        if (err) *err = mdb_strerror(rc);
        env_ = nullptr;
        return false;
    }

    (void)mdb_env_set_maxdbs(env_, 128);
    (void)mdb_env_set_mapsize(env_, 1024ull * 1024ull * 1024ull); // 1 GiB

    rc = mdb_env_open(env_, env_dir_.c_str(), 0, 0664);
    if (rc != MDB_SUCCESS) {
        if (err) *err = mdb_strerror(rc);
        mdb_env_close(env_);
        env_ = nullptr;
        return false;
    }

    env_open_ = true;
    return true;
}

void CdxBackend::close_env_() noexcept {
    if (!env_) return;
    reset_ro_txn_();
    dbis_.clear();
    dbi_ = 0;
    dbi_flags_ = 0;
    env_open_ = false;
    mdb_env_close(env_);
    env_ = nullptr;
}

MDB_txn* CdxBackend::ensure_ro_txn_() const {
    MDB_txn* txn = nullptr;
    const int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn);
    throw_on_mdb_err_(rc, "mdb_txn_begin(ro)");
    return txn;
}

void CdxBackend::reset_ro_txn_() const noexcept {
    auto* self = const_cast<CdxBackend*>(this);
    if (self->ro_txn_) {
        mdb_txn_abort(self->ro_txn_);
        self->ro_txn_ = nullptr;
    }
}

MDB_txn* CdxBackend::begin_rw_txn_() const {
    MDB_txn* txn = nullptr;
    throw_on_mdb_err_(mdb_txn_begin(env_, nullptr, 0, &txn), "mdb_txn_begin(rw)");
    return txn;
}

void CdxBackend::end_txn_abort_(MDB_txn* txn) noexcept {
    if (txn) mdb_txn_abort(txn);
}

void CdxBackend::end_txn_commit_(MDB_txn* txn) {
    throw_on_mdb_err_(mdb_txn_commit(txn), "mdb_txn_commit");
}

bool CdxBackend::open_dbi_for_tag_(MDB_txn* txn,
                                   const std::string& tag_upper,
                                   MDB_dbi& out_dbi,
                                   unsigned int& out_flags,
                                   std::string& err) const {
    out_flags = 0;
    const int rc = mdb_dbi_open(txn, tag_upper.c_str(), 0, &out_dbi);
    if (rc == MDB_NOTFOUND) {
        err = "tag not found";
        return false;
    }
    if (rc != MDB_SUCCESS) {
        err = mdb_strerror(rc);
        return false;
    }
    (void)mdb_dbi_flags(txn, out_dbi, &out_flags);
    return true;
}

std::string CdxBackend::base_key_from_kv_(const MDB_val& k) const {
    if (!composite_mode_) return bytes_to_string_(k);
    if (k.mv_size <= 8) return {};
    return std::string(static_cast<const char*>(k.mv_data),
                       static_cast<const char*>(k.mv_data) + (k.mv_size - 8));
}

bool CdxBackend::decode_recno_from_kv_(const MDB_val& k, const MDB_val& v, std::uint32_t& out_recno) const {
    std::uint64_t rec64 = 0;
    if (decode_u64_le_local(v.mv_data, v.mv_size, rec64)) {
        out_recno = static_cast<std::uint32_t>(rec64);
        return true;
    }
    if (composite_mode_ && k.mv_size >= 8 &&
        decode_u64_le_local(static_cast<const unsigned char*>(k.mv_data) + (k.mv_size - 8), 8, rec64)) {
        out_recno = static_cast<std::uint32_t>(rec64);
        return true;
    }
    return false;
}

bool CdxBackend::open(const std::string& path) {
    cdx_path_ = path;

    std::string env_dir = cdx_path_;
    if (env_dir.size() < 2 || env_dir.substr(env_dir.size() - 2) != ".d") {
        env_dir += ".d";
    }

    std::string err;
    if (!open_env_dir_(env_dir, &err)) return false;

    tag_upper_.clear();
    dbi_ = 0;
    dbi_flags_ = 0;
    composite_mode_ = true;
    return true;
}

void CdxBackend::close() { close_env_(); }

void CdxBackend::rebuild() {
    if (!env_ || tag_upper_.empty() || !area_.isOpen()) {
        return;
    }

    int fld = -1;
    const auto& defs = area_.fields();
    for (int i = 0; i < static_cast<int>(defs.size()); ++i) {
        if (upper_copy_(defs[static_cast<std::size_t>(i)].name) == tag_upper_) {
            fld = i + 1;
            break;
        }
    }
    if (fld < 1) return;

    reset_ro_txn_();
    MDB_txn* txn = begin_rw_txn_();
    try {
        MDB_dbi active_dbi = dbi_;
        if (active_dbi == 0) {
            std::string err;
            unsigned int flags = 0;
            if (!open_dbi_for_tag_(txn, tag_upper_, active_dbi, flags, err)) {
                throw std::runtime_error("rebuild: open active dbi failed: " + err);
            }
        }

        throw_on_mdb_err_(mdb_drop(txn, active_dbi, 0), "mdb_drop(clear tag db)");

        const auto& fdef = defs[static_cast<std::size_t>(fld - 1)];
        const bool is_char = (fdef.type == 'C' || fdef.type == 'c');
        const int keylen = static_cast<int>(fdef.length);

        const int32_t total = area_.recCount();
        for (int32_t rn = 1; rn <= total; ++rn) {
            if (!area_.gotoRec(rn) || !area_.readCurrent()) continue;
            if (area_.isDeleted()) continue;

            std::string s = area_.get(fld);
            if (is_char) {
                for (char& c : s) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
            }

            if (keylen > 0) {
                if (static_cast<int>(s.size()) > keylen) s.resize(static_cast<std::size_t>(keylen));
                if (static_cast<int>(s.size()) < keylen) s.append(static_cast<std::size_t>(keylen - static_cast<int>(s.size())), ' ');
            }

            Key base;
            base.reserve(s.size());
            base.insert(base.end(), s.begin(), s.end());

            const auto storage_key = make_storage_key(composite_mode_, base, static_cast<RecNo>(rn));
            const auto storage_val = make_storage_val(static_cast<RecNo>(rn));

            MDB_val k{ storage_key.size(), const_cast<std::uint8_t*>(storage_key.data()) };
            MDB_val v{ storage_val.size(), const_cast<std::uint8_t*>(storage_val.data()) };

            throw_on_mdb_err_(mdb_put(txn, active_dbi, &k, &v, 0), "mdb_put(rebuild)");
        }

        end_txn_commit_(txn);
        reset_ro_txn_();
        stale_ = false;
    } catch (...) {
        end_txn_abort_(txn);
        reset_ro_txn_();
        throw;
    }
}

void CdxBackend::upsert(const Key& key, RecNo rec) {
    if (!env_ || tag_upper_.empty()) return;

    reset_ro_txn_();
    MDB_txn* txn = begin_rw_txn_();
    try {
        MDB_dbi active_dbi = dbi_;
        if (active_dbi == 0) {
            std::string err;
            unsigned int flags = 0;
            if (!open_dbi_for_tag_(txn, tag_upper_, active_dbi, flags, err)) {
                throw std::runtime_error("upsert: open active dbi failed: " + err);
            }
        }

        const auto storage_key = make_storage_key(composite_mode_, key, rec);
        const auto storage_val = make_storage_val(rec);

        MDB_val k{ storage_key.size(), const_cast<std::uint8_t*>(storage_key.data()) };
        MDB_val v{ storage_val.size(), const_cast<std::uint8_t*>(storage_val.data()) };

        throw_on_mdb_err_(mdb_put(txn, active_dbi, &k, &v, 0), "mdb_put(upsert)");

        end_txn_commit_(txn);
        reset_ro_txn_();
    } catch (...) {
        end_txn_abort_(txn);
        reset_ro_txn_();
        throw;
    }
}

void CdxBackend::erase(const Key& key, RecNo rec) {
    if (!env_ || tag_upper_.empty()) return;

    reset_ro_txn_();
    MDB_txn* txn = begin_rw_txn_();
    try {
        MDB_dbi active_dbi = dbi_;
        if (active_dbi == 0) {
            std::string err;
            unsigned int flags = 0;
            if (!open_dbi_for_tag_(txn, tag_upper_, active_dbi, flags, err)) {
                throw std::runtime_error("erase: open active dbi failed: " + err);
            }
        }

        const auto storage_key = make_storage_key(composite_mode_, key, rec);
        const auto storage_val = make_storage_val(rec);

        MDB_val k{ storage_key.size(), const_cast<std::uint8_t*>(storage_key.data()) };
        MDB_val v{ storage_val.size(), const_cast<std::uint8_t*>(storage_val.data()) };

        int rc = mdb_del(txn, active_dbi, &k, &v);
        if (rc != MDB_SUCCESS && rc != MDB_NOTFOUND) {
            throw_on_mdb_err_(rc, "mdb_del(erase)");
        }

        end_txn_commit_(txn);
        reset_ro_txn_();
    } catch (...) {
        end_txn_abort_(txn);
        reset_ro_txn_();
        throw;
    }
}

void CdxBackend::setTag(const std::string& tag_upper) {
    if (!env_) return;

    const auto up = upper_copy_(tag_upper);
    if (up.empty()) return;

    MDB_txn* ro = nullptr;
    try {
        MDB_dbi dbi = 0;
        unsigned int flags = 0;
        std::string err;

        ro = ensure_ro_txn_();
        if (!open_dbi_for_tag_(ro, up, dbi, flags, err)) {
            mdb_txn_abort(ro);
            ro = nullptr;

            MDB_txn* rw = begin_rw_txn_();
            const int rc = mdb_dbi_open(rw, up.c_str(), MDB_CREATE, &dbi);
            if (rc != MDB_SUCCESS) {
                end_txn_abort_(rw);
                throw_on_mdb_err_(rc, "mdb_dbi_open(MDB_CREATE)");
            }
            (void)mdb_dbi_flags(rw, dbi, &flags);
            end_txn_commit_(rw);
        } else {
            mdb_txn_abort(ro);
            ro = nullptr;
        }

        dbis_[up] = dbi;
        tag_upper_ = up;
        dbi_ = dbi;
        dbi_flags_ = flags;
        composite_mode_ = (dbi_flags_ & MDB_DUPSORT) == 0;
    } catch (...) {
        if (ro) mdb_txn_abort(ro);
        throw;
    }
}

std::unique_ptr<Cursor> CdxBackend::seek(const Key& key) const {
    if (!env_ || tag_upper_.empty()) return {};

    MDB_txn* txn = nullptr;
    try {
        txn = ensure_ro_txn_();

        MDB_dbi dbi = 0;
        unsigned int flags = 0;
        std::string err;
        if (!open_dbi_for_tag_(txn, tag_upper_, dbi, flags, err)) {
            mdb_txn_abort(txn);
            return {};
        }

        const bool composite = (flags & MDB_DUPSORT) == 0;

        std::vector<std::uint8_t> low(key.begin(), key.end());
        if (composite) {
            unsigned char z[8]{0};
            low.insert(low.end(), z, z + 8);
        }

        return std::make_unique<LmdbCursor>(txn, dbi, composite, std::move(low),
                                            std::vector<std::uint8_t>{}, true, false);
    } catch (...) {
        if (txn) mdb_txn_abort(txn);
        throw;
    }
}

std::unique_ptr<Cursor> CdxBackend::scan(const Key& low, const Key& high) const {
    if (!env_ || tag_upper_.empty()) return {};

    MDB_txn* txn = nullptr;
    try {
        txn = ensure_ro_txn_();

        MDB_dbi dbi = 0;
        unsigned int flags = 0;
        std::string err;
        if (!open_dbi_for_tag_(txn, tag_upper_, dbi, flags, err)) {
            mdb_txn_abort(txn);
            return {};
        }

        const bool composite = (flags & MDB_DUPSORT) == 0;

        std::vector<std::uint8_t> lo(low.begin(), low.end());
        std::vector<std::uint8_t> hi(high.begin(), high.end());

        const bool has_lo = !lo.empty();
        const bool has_hi = !hi.empty();

        if (composite) {
            if (has_lo) {
                unsigned char z[8]{0};
                lo.insert(lo.end(), z, z + 8);
            }
            if (has_hi) {
                unsigned char ff[8];
                encode_u64_le_local(~0ull, ff);
                hi.insert(hi.end(), ff, ff + 8);
            }
        }

        return std::make_unique<LmdbCursor>(txn, dbi, composite,
                                            std::move(lo), std::move(hi), has_lo, has_hi);
    } catch (...) {
        if (txn) mdb_txn_abort(txn);
        throw;
    }
}

bool CdxBackend::seekRecnoUserKey(const std::string& user_key, std::uint32_t& out_recno, std::string& out_err) const {
    out_recno = 0;
    out_err.clear();

    if (!env_ || tag_upper_.empty()) {
        out_err = "no active CDX tag";
        return false;
    }

    const auto norm = norm_key_(user_key);
    if (norm.empty()) {
        out_err = "empty key";
        return false;
    }

    MDB_cursor* cur = nullptr;
    MDB_txn* txn = nullptr;

    try {
        txn = ensure_ro_txn_();

        MDB_dbi dbi = 0;
        unsigned int flags = 0;
        std::string err;
        if (!open_dbi_for_tag_(txn, tag_upper_, dbi, flags, err)) {
            mdb_txn_abort(txn);
            out_err = err.empty() ? "tag not found" : err;
            return false;
        }

        const bool composite = (flags & MDB_DUPSORT) == 0;

        std::vector<std::uint8_t> seek_bytes(reinterpret_cast<const std::uint8_t*>(norm.data()),
                                             reinterpret_cast<const std::uint8_t*>(norm.data()) + norm.size());

        if (composite) {
            unsigned char z[8]{0};
            seek_bytes.insert(seek_bytes.end(), z, z + 8);
        }

        throw_on_mdb_err_(mdb_cursor_open(txn, dbi, &cur), "mdb_cursor_open");

        MDB_val k{ seek_bytes.size(), seek_bytes.data() };
        MDB_val v{ 0, nullptr };

        int rc = mdb_cursor_get(cur, &k, &v, MDB_SET_RANGE);
        if (rc == MDB_NOTFOUND) {
            mdb_cursor_close(cur);
            mdb_txn_abort(txn);
            out_err = "not found";
            return false;
        }
        throw_on_mdb_err_(rc, "mdb_cursor_get(MDB_SET_RANGE)");

        if (composite) {
            if (k.mv_size < norm.size() ||
                std::memcmp(k.mv_data, norm.data(), norm.size()) != 0) {
                mdb_cursor_close(cur);
                mdb_txn_abort(txn);
                out_err = "not found";
                return false;
            }
        } else {
            if (k.mv_size != norm.size() ||
                std::memcmp(k.mv_data, norm.data(), norm.size()) != 0) {
                mdb_cursor_close(cur);
                mdb_txn_abort(txn);
                out_err = "not found";
                return false;
            }
        }

        std::uint32_t rec = 0;
        if (!decode_recno_from_kv_(k, v, rec)) {
            mdb_cursor_close(cur);
            mdb_txn_abort(txn);
            out_err = "bad recno encoding";
            return false;
        }

        mdb_cursor_close(cur);
        mdb_txn_abort(txn);

        out_recno = rec;
        return true;
    } catch (const std::exception& e) {
        if (cur) mdb_cursor_close(cur);
        if (txn) mdb_txn_abort(txn);
        out_err = e.what();
        return false;
    }
}

// ---- LmdbCursor -----------------------------------------------------------

CdxBackend::LmdbCursor::LmdbCursor(MDB_txn* ro_txn,
                                   MDB_dbi dbi,
                                   bool composite_mode,
                                   std::vector<std::uint8_t> low,
                                   std::vector<std::uint8_t> high,
                                   bool has_low,
                                   bool has_high)
    : dbi_(dbi),
      composite_(composite_mode),
      low_(std::move(low)),
      high_(std::move(high)),
      has_low_(has_low),
      has_high_(has_high) {
    txn_ = ro_txn;
    throw_on_mdb_err_(mdb_cursor_open(txn_, dbi_, &cur_), "mdb_cursor_open(cursor)");
}

CdxBackend::LmdbCursor::~LmdbCursor() {
    if (cur_) mdb_cursor_close(cur_);
    if (txn_) mdb_txn_abort(txn_);
}

static inline bool decode_recno_from_cursor_key(bool composite, const MDB_val& k, const MDB_val& v, std::uint32_t& out) {
    std::uint64_t rec64 = 0;
    if (decode_u64_le_local(v.mv_data, v.mv_size, rec64)) {
        out = static_cast<std::uint32_t>(rec64);
        return true;
    }
    if (composite && k.mv_size >= 8 &&
        decode_u64_le_local(static_cast<const unsigned char*>(k.mv_data) + (k.mv_size - 8), 8, rec64)) {
        out = static_cast<std::uint32_t>(rec64);
        return true;
    }
    return false;
}

static inline void decode_key_out(bool composite, const MDB_val& k, Key& out) {
    if (composite && k.mv_size > 8) {
        key_from_bytes(out, k.mv_data, k.mv_size - 8);
    } else {
        key_from_bytes(out, k.mv_data, k.mv_size);
    }
}

bool CdxBackend::LmdbCursor::first(Key& outKey, RecNo& outRec) {
    started_ = true;

    MDB_val k{0, nullptr}, v{0, nullptr};
    int rc = MDB_NOTFOUND;

    if (has_low_) {
        k.mv_size = low_.size();
        k.mv_data = low_.data();
        rc = mdb_cursor_get(cur_, &k, &v, MDB_SET_RANGE);
    } else {
        rc = mdb_cursor_get(cur_, &k, &v, MDB_FIRST);
    }

    if (rc == MDB_NOTFOUND) return false;
    throw_on_mdb_err_(rc, "cursor first");

    if (!within_bounds_raw(k, low_, high_, has_low_, has_high_)) return false;

    std::uint32_t r = 0;
    if (!decode_recno_from_cursor_key(composite_, k, v, r)) return false;

    decode_key_out(composite_, k, outKey);
    outRec = static_cast<RecNo>(r);
    return true;
}

bool CdxBackend::LmdbCursor::next(Key& outKey, RecNo& outRec) {
    if (!started_) return first(outKey, outRec);

    MDB_val k{0, nullptr}, v{0, nullptr};
    const int rc = mdb_cursor_get(cur_, &k, &v, MDB_NEXT);
    if (rc == MDB_NOTFOUND) return false;
    throw_on_mdb_err_(rc, "cursor next");

    if (!within_bounds_raw(k, low_, high_, has_low_, has_high_)) return false;

    std::uint32_t r = 0;
    if (!decode_recno_from_cursor_key(composite_, k, v, r)) return false;

    decode_key_out(composite_, k, outKey);
    outRec = static_cast<RecNo>(r);
    return true;
}

bool CdxBackend::LmdbCursor::last(Key& outKey, RecNo& outRec) {
    started_ = true;

    MDB_val k{0, nullptr}, v{0, nullptr};
    int rc = MDB_NOTFOUND;

    if (has_high_) {
        k.mv_size = high_.size();
        k.mv_data = high_.data();
        rc = mdb_cursor_get(cur_, &k, &v, MDB_SET_RANGE);
        if (rc == MDB_NOTFOUND) {
            rc = mdb_cursor_get(cur_, &k, &v, MDB_LAST);
        } else {
            if (cmp_lex(k.mv_data, k.mv_size, high_.data(), high_.size()) > 0) {
                rc = mdb_cursor_get(cur_, &k, &v, MDB_PREV);
            }
        }
    } else {
        rc = mdb_cursor_get(cur_, &k, &v, MDB_LAST);
    }

    if (rc == MDB_NOTFOUND) return false;
    throw_on_mdb_err_(rc, "cursor last");

    if (!within_bounds_raw(k, low_, high_, has_low_, has_high_)) return false;

    std::uint32_t r = 0;
    if (!decode_recno_from_cursor_key(composite_, k, v, r)) return false;

    decode_key_out(composite_, k, outKey);
    outRec = static_cast<RecNo>(r);
    return true;
}

bool CdxBackend::LmdbCursor::prev(Key& outKey, RecNo& outRec) {
    if (!started_) return last(outKey, outRec);

    MDB_val k{0, nullptr}, v{0, nullptr};
    const int rc = mdb_cursor_get(cur_, &k, &v, MDB_PREV);
    if (rc == MDB_NOTFOUND) return false;
    throw_on_mdb_err_(rc, "cursor prev");

    if (!within_bounds_raw(k, low_, high_, has_low_, has_high_)) return false;

    std::uint32_t r = 0;
    if (!decode_recno_from_cursor_key(composite_, k, v, r)) return false;

    decode_key_out(composite_, k, outKey);
    outRec = static_cast<RecNo>(r);
    return true;
}

#endif // XINDEX_HAVE_LMDB

} // namespace xindex