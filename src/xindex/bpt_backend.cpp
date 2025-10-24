#include "xindex/bpt_backend.hpp"

namespace xindex {

// -------- BptMemBackend::MapCursor ------------------------------------------

bool BptMemBackend::MapCursor::first(Key& outKey, RecNo& outRec) {
    it_ = begin_;
    started_ = true;
    if (it_ == end_) return false;
    outKey = it_->first;
    outRec = it_->second;
    return true;
}

bool BptMemBackend::MapCursor::next(Key& outKey, RecNo& outRec) {
    if (!started_) return first(outKey, outRec);
    if (it_ == end_) return false;
    ++it_;
    if (it_ == end_) return false;
    outKey = it_->first;
    outRec = it_->second;
    return true;
}

// -------- BptMemBackend ------------------------------------------------------

void BptMemBackend::upsert(const Key& key, RecNo rec) {
    map_.emplace(key, rec);
}

void BptMemBackend::erase(const Key& key, RecNo rec) {
    auto range = map_.equal_range(key);
    for (auto it = range.first; it != range.second; ) {
        if (it->second == rec) it = map_.erase(it);
        else ++it;
    }
}

std::unique_ptr<Cursor> BptMemBackend::seek(const Key& key) const {
    auto lo = map_.lower_bound(key);
    auto hi = map_.upper_bound(key);
    return std::unique_ptr<Cursor>(new MapCursor(lo, hi));
}

std::unique_ptr<Cursor> BptMemBackend::scan(const Key& low, const Key& high) const {
    auto lo = map_.lower_bound(low);
    auto hi = map_.upper_bound(high);
    return std::unique_ptr<Cursor>(new MapCursor(lo, hi));
}

} // namespace xindex
