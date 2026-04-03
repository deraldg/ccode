#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace xbase { class DbArea; }

namespace xindex::cdxmeta {

struct TableIdentity {
    std::string kind;          // v32, v64, v128, tup
    std::uint8_t version{0};   // DBF version byte
    std::uint32_t rec_len{0};  // record length
    std::uint32_t field_count{0};
    std::uint64_t schema_hash{0};
    std::string source;        // canonical table path
};

struct MetaRecord {
    TableIdentity table;
    std::uint32_t meta_version{1};
    std::string backend{"lmdb"};
};

TableIdentity build_identity(const xbase::DbArea& area);

// Sidecar path is "<container>.meta"
std::optional<MetaRecord> read_meta(const std::string& cdx_path, std::string* err = nullptr);

bool write_meta(const std::string& cdx_path, const MetaRecord& meta, std::string* err = nullptr);

bool matches(const TableIdentity& a, const MetaRecord& b);

} // namespace xindex::cdxmeta