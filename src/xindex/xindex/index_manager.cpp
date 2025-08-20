#include "xindex/index_manager.hpp"
#include <filesystem>
#include <fstream>

namespace xindex {

static std::string baseNameNoExt(const std::string& p) {
    std::filesystem::path ph(p);
    auto stem = ph.stem().string();
    auto dir = ph.parent_path();
    return (dir / stem).string();
}

std::string IndexManager::replaceExt_(const std::string& path, const std::string& newExt) {
    return baseNameNoExt(path) + newExt;
}

void IndexManager::open(const std::string& dbfPath,
                        const KeyDesc& kd,
                        bool allowBuild,
                        std::function<std::optional<std::pair<std::vector<uint8_t>, bool>>(int32_t)> scanner)
{
    key_ = kd;
    idxPath_ = replaceExt_(dbfPath, ".idx");

    // Try load existing
    try {
        load_();
        dirty_ = false;
        return;
    } catch (...) {
        // fallthrough
    }

    if (!allowBuild) throw std::runtime_error("IndexManager: missing/invalid index and build not allowed");

    // Build fresh
    tree_.clear();
    int32_t r = 1;
    for (;; ++r) {
        auto it = scanner(r);
        if (!it) break;
        const auto& [keyBytes, isDeleted] = *it;
        if (!isDeleted && !keyBytes.empty()) tree_.insert(keyBytes, r);
    }
    dirty_ = true;
    save_();
    dirty_ = false;
}

void IndexManager::close() {
    if (dirty_) {
        try { save_(); } catch (...) {}
        dirty_ = false;
    }
}

void IndexManager::insert(const std::vector<uint8_t>& key, int32_t recno) {
    if (key.empty()) return;
    tree_.insert(key, recno);
    dirty_ = true;
}

void IndexManager::erase(const std::vector<uint8_t>& key, int32_t recno) {
    if (key.empty()) return;
    tree_.erase(key, recno);
    dirty_ = true;
}

void IndexManager::update(const std::vector<uint8_t>& oldKey,
                          const std::vector<uint8_t>& newKey,
                          int32_t recno) {
    if (oldKey == newKey) return;
    if (!oldKey.empty()) tree_.erase(oldKey, recno);
    if (!newKey.empty()) tree_.insert(newKey, recno);
    dirty_ = true;
}

std::optional<int32_t> IndexManager::seekGE(const std::vector<uint8_t>& key) const {
    return tree_.seekGE(key);
}

void IndexManager::rebuild(std::function<std::optional<std::pair<std::vector<uint8_t>, bool>>(int32_t)> scanner,
                           int32_t /*recCount*/)
{
    tree_.clear();
    int32_t r = 1;
    for (;; ++r) {
        auto it = scanner(r);
        if (!it) break;
        const auto& [keyBytes, isDeleted] = *it;
        if (!isDeleted && !keyBytes.empty()) tree_.insert(keyBytes, r);
    }
    dirty_ = true;
}

void IndexManager::flush() {
    if (dirty_) save_();
    dirty_ = false;
}

void IndexManager::load_() {
    std::ifstream ifs(idxPath_, std::ios::binary);
    if (!ifs) throw std::runtime_error("IndexManager: cannot open idx for read");
    tree_.load(ifs);
}

void IndexManager::save_() {
    std::ofstream ofs(idxPath_, std::ios::binary | std::ios::trunc);
    if (!ofs) throw std::runtime_error("IndexManager: cannot open idx for write");
    tree_.save(ofs);
}

} // namespace xindex
