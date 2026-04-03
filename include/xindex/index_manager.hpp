// include/xindex/index_manager.hpp
#pragma once
/**
 * @file index_manager.hpp
 * @brief Per-DbArea index ownership + legacy shim used by src/xindex/cli_wrappers.cpp.
 *
 * Current direction:
 *   - Keep the OO API primary.
 *   - Let callers mutate indexes through IndexManager lifecycle hooks.
 *   - Preserve legacy xbase call sites that still pass only recno.
 *   - For now, keyed lifecycle hooks operate only on the currently active backend/tag.
 *   - Multi-tag container maintenance can be added later without changing callers.
 */
#include "xindex/index_backend.hpp"
#include "xindex/cdx_backend.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xbase { class DbArea; }

namespace xindex {

class IndexManager final {
public:
    struct Spec final {
        std::string cdx;
        std::string tag;
    };

    struct ActiveState final {
        Spec spec_;
        const Spec& spec() const noexcept { return spec_; }
    };

    explicit IndexManager(xbase::DbArea& area);
    ~IndexManager();

    IndexManager(const IndexManager&)            = delete;
    IndexManager& operator=(const IndexManager&) = delete;

    // ---- OO core API -------------------------------------------------------
    void close() noexcept;

    bool openCdx(const std::string& cdx_container_path,
                 const std::string& tag_upper = {},
                 std::string* err = nullptr);

    bool setTag(const std::string& tag_upper, std::string* err = nullptr);

    bool isCdx() const noexcept;
    bool hasBackend() const noexcept { return static_cast<bool>(backend_); }
    const std::string& containerPath() const noexcept { return container_path_; }
    std::string activeTag() const;

    std::unique_ptr<Cursor> seek(const Key& key) const;
    std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const;

    bool lmdbSeekUserKey(const std::string& user_key,
                         std::uint32_t& out_recno,
                         std::string& out_err) const;

    // ---- Canonical active-tag key helpers ---------------------------------
    int  activeTagFieldIndex1() const;
    bool activeTagMatchesField(int field1) const;

    Key buildActiveTagBaseKeyFromString(const std::string& raw_value) const;
    Key buildActiveTagBaseKeyFromCurrentRecord() const;

    // ---- OO mutation wrappers ---------------------------------------------
    bool replace_active_field_value(int field1,
                                    const std::string& old_value,
                                    const std::string& new_value,
                                    RecNo rec);

    bool append_active_field_value(int field1,
                                   const std::string& value,
                                   RecNo rec);

    bool delete_active_field_value(int field1,
                                   const std::string& value,
                                   RecNo rec);

    // ---- Legacy shim API ---------------------------------------------------
    bool set_active(const std::string& tagName);
    std::optional<ActiveState> active() const;
    bool has_active() const { return hasBackend(); }
    void clear_active() { close(); }
    std::vector<std::string> listTags() const;
    bool load_for_table(const std::string& path_or_dbf);
    bool load_json(const std::string& inx_path);
    void set_direction(bool /*descending*/) noexcept {}

    // ---- Record lifecycle hooks -------------------------------------------
    void on_append(const Key& key, RecNo rec) {
        if (!backend_) return;
        backend_->upsert(key, rec);
    }

    void on_delete(const Key& key, RecNo rec) {
        if (!backend_) return;
        backend_->erase(key, rec);
    }

    void on_replace(const Key& old_key,
                    const Key& new_key,
                    RecNo rec) {
        if (!backend_) return;
        if (old_key == new_key) return;

        // Safer sequence with current backend contract:
        // insert new key first, then remove old key.
        // If erase fails, attempt to roll back the new key.
        backend_->upsert(new_key, rec);
        try {
            backend_->erase(old_key, rec);
        } catch (...) {
            try {
                backend_->erase(new_key, rec);
            } catch (...) {
            }
            throw;
        }
    }

    // ---- Legacy compatibility overloads -----------------------------------
    void on_append(RecNo /*rec*/) {}
    void on_delete(RecNo /*rec*/) {}
    void on_replace(RecNo /*rec*/) {}

private:
    xbase::DbArea& area_;
    std::unique_ptr<IIndexBackend> backend_{};
    std::string container_path_{};
    std::string tag_upper_{};

    static std::string to_upper_copy_ascii_(std::string s);
};

} // namespace xindex