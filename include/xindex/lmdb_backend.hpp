#pragma once
// include/xindex/lmdb_backend.hpp
//
// LMDB backend for DotTalk++ xindex abstraction.
//
// Storage layout:
//   - One unnamed DB (dbi_)
//   - MDB_DUPSORT + MDB_DUPFIXED
//     key = Key bytes
//     val = RecNo packed as 8 bytes LE
//
// Notes:
//   - This backend does not filter deleted records; consumer must do DBF-level filtering.
//   - Designed to match xindex/index_backend.hpp contract exactly.

#include "xindex/index_backend.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct MDB_env;
struct MDB_txn;
struct MDB_cursor;

namespace xindex {

class LmdbBackend final : public IIndexBackend {
public:
    LmdbBackend();
    ~LmdbBackend() override;

    bool open(const std::string& path) override;
    void close() override;

    void setFingerprint(std::uint32_t fp) override;
    bool wasStale() const override { return stale_; }

    void rebuild() override;

    void upsert(const Key& key, RecNo rec) override;
    void erase (const Key& key, RecNo rec) override;

    std::unique_ptr<Cursor> seek(const Key& key) const override;
    std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const override;

private:
    // LMDB handle state
    MDB_env* env_{nullptr};
    unsigned int dbi_{0};      // MDB_dbi is typedef'd to unsigned int
    bool opened_{false};

    std::string path_{};
    std::uint32_t fingerprint_{0};
    bool stale_{false};

private:
    // Helpers
    static std::vector<std::uint8_t> pack_recno(RecNo rec);
    static RecNo unpack_recno(const void* p, std::size_t n);

    static void throw_on_mdb_err(int rc, const char* what);

    void ensure_opened_() const;
    void open_db_(MDB_txn* txn);

private:
    class LmdbCursor final : public Cursor {
    public:
        LmdbCursor(MDB_env* env,
                   unsigned int dbi,
                   std::vector<std::uint8_t> low_key,
                   std::vector<std::uint8_t> high_key,
                   bool has_low,
                   bool has_high);

        ~LmdbCursor() override;

        bool first(Key& outKey, RecNo& outRec) override;
        bool next (Key& outKey, RecNo& outRec) override;
        bool last (Key& outKey, RecNo& outRec) override;
        bool prev (Key& outKey, RecNo& outRec) override;

    private:
        MDB_env* env_{nullptr};
        unsigned int dbi_{0};

        MDB_txn* txn_{nullptr};
        MDB_cursor* cur_{nullptr};

        bool started_{false};

        std::vector<std::uint8_t> low_{};
        std::vector<std::uint8_t> high_{};
        bool has_low_{false};
        bool has_high_{false};

    private:
        bool step_(int op, Key& outKey, RecNo& outRec);
        bool within_bounds_(const void* keyp, std::size_t keyn) const;

        bool position_first_(Key& outKey, RecNo& outRec);
        bool position_last_(Key& outKey, RecNo& outRec);

        static int keycmp_bytes_(const void* a, std::size_t an,
                                 const void* b, std::size_t bn);
    };
};

} // namespace xindex