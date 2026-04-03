// include/xindex/cdx_backend.hpp
#pragma once
/**
 * @file xindex/cdx_backend.hpp
 * @brief CDX container backend implemented as an LMDB environment (<container>.cdx.d) with one named DB per TAG.
 *
 * Ownership:
 *   DbArea -> IndexManager -> CdxBackend -> MDB_env*
 *
 * Notes:
 * - To avoid MDB_BAD_RSLOT from nested read-only transactions, this backend keeps a single shared RO transaction
 *   (ro_txn_) per backend instance. All cursor operations and LMDB SEEK reuse that transaction.
 * - Write transactions are created per-operation (future incremental maintenance).
 */
#include "xindex/index_backend.hpp"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#if __has_include(<lmdb.h>)
  #include <lmdb.h>
  #define XINDEX_HAVE_LMDB 1
#else
  #define XINDEX_HAVE_LMDB 0
  struct MDB_env;
  struct MDB_txn;
  struct MDB_cursor;
  using MDB_dbi = unsigned int;
  struct MDB_val { size_t mv_size; void* mv_data; };
#endif

namespace xbase { class DbArea; }

namespace xindex {

struct ITagBackend : public IIndexBackend {
    virtual void setTag(const std::string& tag_upper) = 0;
    virtual std::string activeTag() const = 0;
};

class CdxBackend final : public ITagBackend {
public:
    CdxBackend(xbase::DbArea& area, std::string cdx_container_path);
    ~CdxBackend() override;

    CdxBackend(const CdxBackend&)            = delete;
    CdxBackend& operator=(const CdxBackend&) = delete;

    bool open(const std::string& path) override;
    void close() override;

    void setFingerprint(std::uint32_t fp) override { fingerprint_ = fp; }
    bool wasStale() const override { return stale_; }

    void rebuild() override;

    void upsert(const Key& key, RecNo rec) override;
    void erase (const Key& key, RecNo rec) override;

    std::unique_ptr<Cursor> seek(const Key& key) const override;
    std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const override;

    void setTag(const std::string& tag_upper) override;
    std::string activeTag() const override { return tag_upper_; }

    // Diagnostics helpers (CLI)
#if XINDEX_HAVE_LMDB
    bool isOpen() const noexcept { return env_ != nullptr; }
    bool compositeMode() const noexcept { return composite_mode_; }
#else
    bool isOpen() const noexcept { return false; }
    bool compositeMode() const noexcept { return false; }
#endif
    const std::string& envDir() const noexcept { return env_dir_; }

    bool seekRecnoUserKey(const std::string& user_key, std::uint32_t& out_recno, std::string& out_err) const;

private:
    xbase::DbArea& area_;

    std::string cdx_path_;
    std::string env_dir_;

    std::string tag_upper_;
    std::uint32_t fingerprint_{0};
    bool stale_{false};

#if XINDEX_HAVE_LMDB
    MDB_env* env_{nullptr};
    bool env_open_{false};

    // One shared read-only transaction per backend (prevents MDB_BAD_RSLOT from nested ro txns).
    mutable MDB_txn* ro_txn_{nullptr};

    // dbi cache by tag name (upper)
    std::unordered_map<std::string, MDB_dbi> dbis_{};
    MDB_dbi dbi_{0};
    unsigned int dbi_flags_{0};
    bool composite_mode_{true};

    static void throw_on_mdb_err_(int rc, const char* what);

    static std::string trim_copy_(const std::string& s);
    static std::string upper_copy_(std::string s);
    static void rtrim_lmdb_key_(std::string& s);
    static std::string norm_key_(std::string s);

    static void encode_u64_le_(std::uint64_t v, unsigned char out8[8]);
    static bool decode_u64_le_(const void* p, size_t n, std::uint64_t& out);

    static std::string bytes_to_string_(const MDB_val& v);

    bool open_env_dir_(const std::string& env_dir, std::string* err);
    void close_env_() noexcept;

    MDB_txn* ensure_ro_txn_() const;
    void reset_ro_txn_() const noexcept;

    MDB_txn* begin_rw_txn_() const;
    static void end_txn_abort_(MDB_txn* txn) noexcept;
    static void end_txn_commit_(MDB_txn* txn);

    bool open_dbi_for_tag_(MDB_txn* txn,
                           const std::string& tag_upper,
                           MDB_dbi& out_dbi,
                           unsigned int& out_flags,
                           std::string& err) const;

    std::string base_key_from_kv_(const MDB_val& k) const;
    bool decode_recno_from_kv_(const MDB_val& k, const MDB_val& v, std::uint32_t& out_recno) const;

    class LmdbCursor final : public Cursor {
    public:
        LmdbCursor(MDB_txn* ro_txn,
                   MDB_dbi dbi,
                   bool composite_mode,
                   std::vector<std::uint8_t> low,
                   std::vector<std::uint8_t> high,
                   bool has_low,
                   bool has_high);
        ~LmdbCursor() override;

        bool first(Key& outKey, RecNo& outRec) override;
        bool next (Key& outKey, RecNo& outRec) override;
        bool last (Key& outKey, RecNo& outRec) override;
        bool prev (Key& outKey, RecNo& outRec) override;

    private:
        MDB_dbi  dbi_{0};
        bool composite_{true};

        MDB_txn* txn_{nullptr};  // non-owning; owned by CdxBackend::ro_txn_
        MDB_cursor* cur_{nullptr};

        bool started_{false};

        std::vector<std::uint8_t> low_{};
        std::vector<std::uint8_t> high_{};
        bool has_low_{false};
        bool has_high_{false};
    };
#else
    void close_env_() noexcept {}
#endif
};

} // namespace xindex
