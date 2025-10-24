#include "xindex/bptree_backend.hpp"

#include <fstream>
#include <ios>
#include <algorithm> // for std::copy

namespace xindex {

// -------- BPlusTreeBackend::MapCursor ----------------------------------------

bool BPlusTreeBackend::MapCursor::first(Key& outKey, RecNo& outRec) {
    it_ = begin_;
    started_ = true;
    if (it_ == end_) return false;
    outKey = it_->first;
    outRec = it_->second;
    return true;
}

bool BPlusTreeBackend::MapCursor::next(Key& outKey, RecNo& outRec) {
    if (!started_) return first(outKey, outRec);
    if (it_ == end_) return false;
    ++it_;
    if (it_ == end_) return false;
    outKey = it_->first;
    outRec = it_->second;
    return true;
}

// -------- small helpers ------------------------------------------------------

void BPlusTreeBackend::write_u32(std::ostream& os, std::uint32_t v) {
    os.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

uint32_t BPlusTreeBackend::read_u32(std::istream& is) {
    std::uint32_t v{};
    is.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

// -------- BPlusTreeBackend ----------------------------------------------------

bool BPlusTreeBackend::open(const std::string& path) {
    path_ = path;
    stale_ = false;
    // Try to load; if file missing, that's fine.
    load(path_);
    return true;
}

void BPlusTreeBackend::close() {
    // Save when closing, best-effort
    if (!path_.empty()) {
        saveToFile(path_);
    }
    path_.clear();
}

void BPlusTreeBackend::upsert(const Key& key, RecNo rec) {
    map_.emplace(key, rec);
}

void BPlusTreeBackend::erase(const Key& key, RecNo rec) {
    auto range = map_.equal_range(key);
    for (auto it = range.first; it != range.second; ) {
        if (it->second == rec) it = map_.erase(it);
        else ++it;
    }
}

std::unique_ptr<Cursor> BPlusTreeBackend::seek(const Key& key) const {
    auto lo = map_.lower_bound(key);
    auto hi = map_.upper_bound(key);
    return std::unique_ptr<Cursor>(new MapCursor(lo, hi));
}

std::unique_ptr<Cursor> BPlusTreeBackend::scan(const Key& low, const Key& high) const {
    auto lo = map_.lower_bound(low);
    auto hi = map_.upper_bound(high);
    return std::unique_ptr<Cursor>(new MapCursor(lo, hi));
}

// -------- persistence (trivial binary) ---------------------------------------

bool BPlusTreeBackend::saveToFile(const std::string& path) const {
    std::ofstream os(path, std::ios::binary);
    if (!os) return false;

    // magic
    os.write("XIDX", 4);

    // count
    write_u32(os, static_cast<std::uint32_t>(map_.size()));

    for (const auto& kv : map_) {
        const auto& k = kv.first;
        const auto  r = kv.second;

        write_u32(os, static_cast<std::uint32_t>(k.size()));
        if (!k.empty()) os.write(reinterpret_cast<const char*>(k.data()), static_cast<std::streamsize>(k.size()));
        write_u32(os, static_cast<std::uint32_t>(r));
    }
    return true;
}

bool BPlusTreeBackend::load(const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) return false;

    char magic[4] = {};
    is.read(magic, 4);
    if (is.gcount() != 4 || magic[0] != 'X' || magic[1] != 'I' || magic[2] != 'D' || magic[3] != 'X') {
        // Not our format, leave map_ untouched.
        return false;
    }

    const std::uint32_t count = read_u32(is);
    Map tmp;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t ksz = read_u32(is);
        Key k;
        k.resize(ksz);
        if (ksz) is.read(reinterpret_cast<char*>(k.data()), static_cast<std::streamsize>(ksz));
        const std::uint32_t r = read_u32(is);
        tmp.emplace(std::move(k), static_cast<RecNo>(r));
    }

    map_.swap(tmp);
    return true;
}

} // namespace xindex
