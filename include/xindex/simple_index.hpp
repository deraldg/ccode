// include/xindex/simple_index.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace xbase { class DbArea; }

namespace xindex {

// What we store alongside an index file (.inx)
struct IndexMeta {
    bool ascending{true};                 // ASC/DESC (only ASC in compare if you want)
    std::vector<int> field_indices;       // which DbArea fields participate (no MEMO)
    std::string expression;               // original expression text (placeholder-supported)
};

// An in-memory entry (recno + key)
struct IndexEntry {
    uint32_t recno{};
    std::string key;                      // normalized string key for sort/compare
};

// A minimal index that can be built, opened, saved.
class SimpleIndex {
public:
    // Build from a DBF scan using 'meta' and save to 'inxPath'.
    // Returns false on error and fills 'err'.
    static bool build_and_save(xbase::DbArea& A,
                               const IndexMeta& meta,
                               const std::filesystem::path& inxPath,
                               std::string* err = nullptr);

    // Open an existing .inx (load meta + entries).
    bool open(const std::filesystem::path& inxPath, std::string* err = nullptr);

    // Save current meta + entries to a .inx file.
    bool save(const std::filesystem::path& inxPath, std::string* err = nullptr) const;

    // Read-only access
    const IndexMeta& meta()   const { return meta_; }
    const std::vector<IndexEntry>& entries() const { return entries_; }

    // Rebuild from a DBF and keep in memory (does not save).
    bool rebuild_from_db(xbase::DbArea& A, const IndexMeta& meta, std::string* err = nullptr);

private:
    static bool validate_meta(const xbase::DbArea& A, const IndexMeta& meta, std::string* err);
    static std::string build_key_for_record(const xbase::DbArea& A,
                                            const IndexMeta& meta);

    static void assign_err(std::string* e, const std::string& m) { if (e) *e = m; }

    IndexMeta meta_;
    std::vector<IndexEntry> entries_;
};

} // namespace xindex
