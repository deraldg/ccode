#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <cstdint>
#include <fstream>
#include "xindex/bptree.hpp"

namespace xindex {

struct KeyDesc {
    // For v1 we’ll just use the first CHAR field (already supported by xbase::DbArea::firstCharField()).
    // This struct leaves room to expand (compound keys, types) later.
    std::string name;
};

class IndexManager {
public:
    IndexManager() = default;
    ~IndexManager() { close(); }

    // Open or create index file next to DBF. If file absent and allowBuild, rebuild via scanner().
    // scanner(recno) must return {keyBytes, isDeleted}. It will be called from 1..recCount.
    void open(const std::string& dbfPath,
              const KeyDesc& key,
              bool allowBuild,
              std::function<std::optional<std::pair<std::vector<uint8_t>, bool>>(int32_t)> scanner);

    void close();

    // Point ops from record lifecycle
    void insert(const std::vector<uint8_t>& key, int32_t recno);
    void erase (const std::vector<uint8_t>& key, int32_t recno);
    void update(const std::vector<uint8_t>& oldKey,
                const std::vector<uint8_t>& newKey,
                int32_t recno);

    // Navigation (basic)
    std::optional<int32_t> seekGE(const std::vector<uint8_t>& key) const;

    // Maintenance
    void rebuild(std::function<std::optional<std::pair<std::vector<uint8_t>, bool>>(int32_t)> scanner,
                 int32_t recCount);

    // Persist now
    void flush();

    // File path used
    const std::string& idxPath() const { return idxPath_; }

private:
    std::string idxPath_;
    KeyDesc     key_;
    BPlusTree   tree_;
    bool        dirty_{false};

    static std::string replaceExt_(const std::string& path, const std::string& newExt);
    void load_();
    void save_();
};

} // namespace xindex
