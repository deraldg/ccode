#include "xindex/index_tag.hpp"

namespace xindex {

IndexTag::IndexTag(IndexSpec spec) : spec_(std::move(spec)) {}
void IndexTag::clear() { entries_.clear(); }

void IndexTag::bulk_build(std::vector<KeyRec>&& entries) {
    entries_ = std::move(entries);
    std::sort(entries_.begin(), entries_.end(), PairLess{});
}

void IndexTag::insert(IndexKey key, int recno) {
    KeyRec kr{ std::move(key), recno };
    auto it = std::lower_bound(entries_.begin(), entries_.end(), kr, PairLess{});
    entries_.insert(it, std::move(kr));
}

void IndexTag::erase_recno(int recno) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [recno](KeyRec const& kr){ return kr.recno == recno; }), entries_.end());
}

std::optional<int> IndexTag::seek_first_ge(IndexKey const& key) const {
    if (entries_.empty()) return std::nullopt;
    auto it = std::lower_bound(entries_.begin(), entries_.end(), key, PairLess{});
    if (it == entries_.end()) return std::nullopt;
    return it->recno;
}

int IndexTag::top() const { return entries_.empty() ? -1 : entries_.front().recno; }
int IndexTag::bottom() const { return entries_.empty() ? -1 : entries_.back().recno; }

} // namespace xindex
