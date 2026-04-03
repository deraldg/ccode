#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "xindex/index_backend.hpp"
#include "xindex/key_common.hpp"
#include "xindex/key_codec.hpp"

namespace xbase { class DbArea; }

namespace xindex {

class CnxBackend : public IIndexBackend {
public:
    CnxBackend(xbase::DbArea& area, std::string cnx_path, std::string tag_upper);
    ~CnxBackend() override = default;

    bool open(const std::string& path) override;
    void close() override;

    void setFingerprint(std::uint32_t fp) override { fingerprint_ = fp; }
    bool wasStale() const override { return stale_; }

    void rebuild() override;

    void upsert(const Key& key, RecNo rec) override;
    void erase (const Key& key, RecNo rec) override;

    std::unique_ptr<Cursor> seek(const Key& key)  const override;
    std::unique_ptr<Cursor> scan(const Key& low, const Key& high) const override;

    const std::string& path() const { return cnx_path_; }
    const std::string& tag () const { return tag_upper_; }

private:
    struct TreeCursor;
    using Multimap = std::multimap<Key, RecNo, KeyLess>;

    xbase::DbArea& area_;
    std::string cnx_path_;
    std::string tag_upper_;
    char tag_type_{'C'};           // DBF field type for this tag (C/N/D/L/etc.)
    std::uint32_t fingerprint_{0};
    bool stale_{false};

    Multimap tree_;

    Key make_key_from_record(std::uint32_t rec) const;
    static Key make_key_from_string(const std::string& s);

    // Future on-disk paging hooks (stubs)
    void load_from_cnx();   // no-op now
    void save_to_cnx();     // no-op now
};

} // namespace xindex



