// src/xindex/lmdb_backend.cpp
#include "xindex/lmdb_backend.hpp"

#include <lmdb.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace xindex {

LmdbBackend::LmdbBackend() = default;

LmdbBackend::~LmdbBackend() {
    close();
}

// ---------------- helpers ----------------------------------------------------

std::vector<std::uint8_t> LmdbBackend::pack_recno(RecNo rec) {
    // store as uint64 LE, fixed 8 bytes
    std::uint64_t v = static_cast<std::uint64_t>(rec);
    std::vector<std::uint8_t> bytes(sizeof(v));
    std::memcpy(bytes.data(), &v, sizeof(v));
    return bytes;
}

RecNo LmdbBackend::unpack_recno(const void* p, std::size_t n) {
    if (!p || n != sizeof(std::uint64_t)) return static_cast<RecNo>(0);
    std::uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return static_cast<RecNo>(v);
}

void LmdbBackend::throw_on_mdb_err(int rc, const char* what) {
    if (rc != MDB_SUCCESS) {
        const char* msg = mdb_strerror(rc);
        throw std::runtime_error(std::string(what) + ": " + (msg ? msg : "LMDB error"));
    }
}

void LmdbBackend::ensure_opened_() const {
    if (!opened_ || !env_) {
        throw std::runtime_error("LMDB backend not open");
    }
}

void LmdbBackend::open_db_(MDB_txn* txn) {
    // unnamed DB; duplicates enabled; fixed-size dup values
    int rc = mdb_dbi_open(
        txn,
        nullptr,
        MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED,
        reinterpret_cast<MDB_dbi*>(&dbi_));
    throw_on_mdb_err(rc, "mdb_dbi_open");
}

// ---------------- lifecycle -------------------------------------------------

bool LmdbBackend::open(const std::string& path) {
    if (opened_) close();

    path_ = path;

    int rc = mdb_env_create(reinterpret_cast<MDB_env**>(&env_));
    if (rc != MDB_SUCCESS || !env_) return false;

    // Reasonable defaults; adjust later via config if desired.
    (void)mdb_env_set_mapsize(env_, 1024ULL * 1024ULL * 1024ULL); // 1 GiB
    (void)mdb_env_set_maxdbs(env_, 128);

    // On Windows/MSVC shells that reuse threads, MDB_NOTLS prevents reader slot reuse issues.
    const unsigned int flags = MDB_NOTLS;

    rc = mdb_env_open(env_, path.c_str(), flags, 0664);
    if (rc != MDB_SUCCESS) {
        mdb_env_close(env_);
        env_ = nullptr;
        return false;
    }

    MDB_txn* txn = nullptr;
    rc = mdb_txn_begin(env_, nullptr, 0, &txn);
    if (rc != MDB_SUCCESS || !txn) {
        mdb_env_close(env_);
        env_ = nullptr;
        return false;
    }

    try {
        open_db_(txn);
        throw_on_mdb_err(mdb_txn_commit(txn), "mdb_txn_commit(open)");
    } catch (...) {
        mdb_txn_abort(txn);
        mdb_env_close(env_);
        env_ = nullptr;
        return false;
    }

    opened_ = true;
    return true;
}

void LmdbBackend::close() {
    if (env_) {
        if (dbi_ != 0) {
            mdb_dbi_close(env_, static_cast<MDB_dbi>(dbi_));
        }
        mdb_env_close(env_);
        env_ = nullptr;
        dbi_ = 0;
    }
    opened_ = false;
}

void LmdbBackend::setFingerprint(std::uint32_t fp) {
    fingerprint_ = fp;
    stale_ = true;
}

// ---------------- core ops --------------------------------------------------

void LmdbBackend::upsert(const Key& key, RecNo rec) {
    ensure_opened_();

    MDB_txn* txn = nullptr;
    int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
    throw_on_mdb_err(rc, "mdb_txn_begin(upsert)");

    MDB_val k{};
    k.mv_size = key.size();
    k.mv_data = const_cast<std::uint8_t*>(key.data());

    auto vbytes = pack_recno(rec);
    MDB_val v{};
    v.mv_size = vbytes.size();
    v.mv_data = vbytes.data();

    rc = mdb_put(txn, static_cast<MDB_dbi>(dbi_), &k, &v, 0);
    if (rc == MDB_SUCCESS) {
        rc = mdb_txn_commit(txn);
        if (rc != MDB_SUCCESS) {
            mdb_txn_abort(txn);
            throw_on_mdb_err(rc, "mdb_txn_commit(upsert)");
        }
        return;
    }

    mdb_txn_abort(txn);
    throw_on_mdb_err(rc, "mdb_put(upsert)");
}

void LmdbBackend::erase(const Key& key, RecNo rec) {
    ensure_opened_();

    MDB_txn* txn = nullptr;
    int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
    throw_on_mdb_err(rc, "mdb_txn_begin(erase)");

    MDB_val k{};
    k.mv_size = key.size();
    k.mv_data = const_cast<std::uint8_t*>(key.data());

    auto vbytes = pack_recno(rec);
    MDB_val v{};
    v.mv_size = vbytes.size();
    v.mv_data = vbytes.data();

    rc = mdb_del(txn, static_cast<MDB_dbi>(dbi_), &k, &v);
    if (rc == MDB_NOTFOUND) {
        mdb_txn_abort(txn);
        return;
    }

    if (rc == MDB_SUCCESS) {
        rc = mdb_txn_commit(txn);
        if (rc != MDB_SUCCESS) {
            mdb_txn_abort(txn);
            throw_on_mdb_err(rc, "mdb_txn_commit(erase)");
        }
        return;
    }

    mdb_txn_abort(txn);
    throw_on_mdb_err(rc, "mdb_del(erase)");
}

void LmdbBackend::rebuild() {
    ensure_opened_();
    stale_ = false;

    // Placeholder rebuild:
    // - Clear DB contents (mdb_drop with del=0)
    // - Leave actual DBF scan/reinsert to the caller/planner
    MDB_txn* txn = nullptr;
    int rc = mdb_txn_begin(env_, nullptr, 0, &txn);
    throw_on_mdb_err(rc, "mdb_txn_begin(rebuild)");

    rc = mdb_drop(txn, static_cast<MDB_dbi>(dbi_), 0);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        throw_on_mdb_err(rc, "mdb_drop(rebuild)");
    }

    // Optional tiny sentinel insert can be removed; kept OFF by default.
    // (If you want it, make it conditional with a dev flag.)

    rc = mdb_txn_commit(txn);
    if (rc != MDB_SUCCESS) {
        mdb_txn_abort(txn);
        throw_on_mdb_err(rc, "mdb_txn_commit(rebuild)");
    }
}

// ---------------- cursor factory --------------------------------------------

std::unique_ptr<Cursor> LmdbBackend::seek(const Key& key) const {
    ensure_opened_();
    return std::make_unique<LmdbCursor>(
        env_,
        dbi_,
        key, std::vector<std::uint8_t>{},
        true, false);
}

std::unique_ptr<Cursor> LmdbBackend::scan(const Key& low, const Key& high) const {
    ensure_opened_();
    return std::make_unique<LmdbCursor>(
        env_,
        dbi_,
        low, high,
        true, true);
}

// ---------------- LmdbCursor ------------------------------------------------

LmdbBackend::LmdbCursor::LmdbCursor(MDB_env* env,
                                    unsigned int dbi,
                                    std::vector<std::uint8_t> low_key,
                                    std::vector<std::uint8_t> high_key,
                                    bool has_low,
                                    bool has_high)
    : env_(env),
      dbi_(dbi),
      low_(std::move(low_key)),
      high_(std::move(high_key)),
      has_low_(has_low),
      has_high_(has_high) {}

LmdbBackend::LmdbCursor::~LmdbCursor() {
    if (cur_) mdb_cursor_close(cur_);
    if (txn_) mdb_txn_abort(txn_);
    cur_ = nullptr;
    txn_ = nullptr;
}

bool LmdbBackend::LmdbCursor::first(Key& outKey, RecNo& outRec) {
    return position_first_(outKey, outRec);
}

bool LmdbBackend::LmdbCursor::next(Key& outKey, RecNo& outRec) {
    return step_(MDB_NEXT, outKey, outRec);
}

bool LmdbBackend::LmdbCursor::last(Key& outKey, RecNo& outRec) {
    return position_last_(outKey, outRec);
}

bool LmdbBackend::LmdbCursor::prev(Key& outKey, RecNo& outRec) {
    return step_(MDB_PREV, outKey, outRec);
}

bool LmdbBackend::LmdbCursor::within_bounds_(const void* keyp, std::size_t keyn) const {
    if (has_high_) {
        if (keycmp_bytes_(keyp, keyn, high_.data(), high_.size()) > 0) return false;
    }
    return true;
}

bool LmdbBackend::LmdbCursor::step_(int op, Key& outKey, RecNo& outRec) {
    if (!cur_) return false;

    MDB_val k{}, v{};
    int rc = mdb_cursor_get(cur_, &k, &v, static_cast<MDB_cursor_op>(op));
    if (rc == MDB_NOTFOUND) return false;
    if (rc != MDB_SUCCESS) return false;

    if (!within_bounds_(k.mv_data, k.mv_size)) return false;

    outKey.assign(
        static_cast<const std::uint8_t*>(k.mv_data),
        static_cast<const std::uint8_t*>(k.mv_data) + k.mv_size);

    outRec = LmdbBackend::unpack_recno(v.mv_data, v.mv_size);
    return true;
}

bool LmdbBackend::LmdbCursor::position_first_(Key& outKey, RecNo& outRec) {
    // reset
    if (cur_) { mdb_cursor_close(cur_); cur_ = nullptr; }
    if (txn_) { mdb_txn_abort(txn_); txn_ = nullptr; }
    started_ = false;

    int rc = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn_);
    if (rc != MDB_SUCCESS) return false;

    rc = mdb_cursor_open(txn_, static_cast<MDB_dbi>(dbi_), &cur_);
    if (rc != MDB_SUCCESS || !cur_) {
        mdb_txn_abort(txn_);
        txn_ = nullptr;
        return false;
    }

    MDB_val k{}, v{};
    if (has_low_) {
        k.mv_size = low_.size();
        k.mv_data = low_.data();
        rc = mdb_cursor_get(cur_, &k, &v, MDB_SET_RANGE);
    } else {
        rc = mdb_cursor_get(cur_, &k, &v, MDB_FIRST);
    }

    if (rc == MDB_NOTFOUND || rc != MDB_SUCCESS) return false;
    if (!within_bounds_(k.mv_data, k.mv_size)) return false;

    outKey.assign(
        static_cast<const std::uint8_t*>(k.mv_data),
        static_cast<const std::uint8_t*>(k.mv_data) + k.mv_size);

    outRec = LmdbBackend::unpack_recno(v.mv_data, v.mv_size);
    started_ = true;
    return true;
}

bool LmdbBackend::LmdbCursor::position_last_(Key& outKey, RecNo& outRec) {
    // Ensure we're positioned with a live txn/cursor
    if (!cur_) {
        if (!position_first_(outKey, outRec)) return false;
    }

    MDB_val k{}, v{};
    int rc = MDB_SUCCESS;

    if (has_high_) {
        // seek to first >= high, then step back if needed
        k.mv_size = high_.size();
        k.mv_data = high_.data();

        rc = mdb_cursor_get(cur_, &k, &v, MDB_SET_RANGE);
        if (rc == MDB_NOTFOUND) {
            // no key >= high, so last might still be <= high => go MDB_LAST and bound-check
            rc = mdb_cursor_get(cur_, &k, &v, MDB_LAST);
            if (rc != MDB_SUCCESS) return false;
        } else if (rc == MDB_SUCCESS) {
            // if current key > high, go prev
            if (keycmp_bytes_(k.mv_data, k.mv_size, high_.data(), high_.size()) > 0) {
                rc = mdb_cursor_get(cur_, &k, &v, MDB_PREV);
                if (rc != MDB_SUCCESS) return false;
            }
        } else {
            return false;
        }
    } else {
        rc = mdb_cursor_get(cur_, &k, &v, MDB_LAST);
        if (rc != MDB_SUCCESS) return false;
    }

    if (!within_bounds_(k.mv_data, k.mv_size)) return false;

    outKey.assign(
        static_cast<const std::uint8_t*>(k.mv_data),
        static_cast<const std::uint8_t*>(k.mv_data) + k.mv_size);

    outRec = LmdbBackend::unpack_recno(v.mv_data, v.mv_size);
    return true;
}

int LmdbBackend::LmdbCursor::keycmp_bytes_(const void* a, std::size_t an,
                                          const void* b, std::size_t bn) {
    const std::size_t n = std::min(an, bn);
    int cmp = std::memcmp(a, b, n);
    if (cmp != 0) return cmp;
    if (an == bn) return 0;
    return (an < bn) ? -1 : 1;
}

} // namespace xindex