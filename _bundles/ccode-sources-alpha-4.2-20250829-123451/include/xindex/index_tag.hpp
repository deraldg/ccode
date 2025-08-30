#pragma once
#include "xindex/index_spec.hpp"
#include "xindex/index_key.hpp"
#include <vector>
#include <optional>
#include <algorithm>

namespace xindex {

struct KeyRec {
    IndexKey key;
    int recno = -1;
};

class IndexTag {
public:
    explicit IndexTag(IndexSpec spec);
    IndexSpec const& spec() const { return spec_; }
    IndexSpec&       spec()       { return spec_; }

    void clear();
    void bulk_build(std::vector<KeyRec>&& entries);
    void insert(IndexKey key, int recno);
    void erase_recno(int recno);

    std::optional<int> seek_first_ge(IndexKey const& key) const;

    const std::vector<KeyRec>& entries() const { return entries_; }
    int top() const;
    int bottom() const;

private:
    IndexSpec              spec_;
    std::vector<KeyRec>    entries_;

    struct PairLess {
        bool operator()(KeyRec const& a, KeyRec const& b) const {
            int c = cmp_key(a.key, b.key);
            if (c != 0) return c < 0;
            return a.recno < b.recno;
        }
        bool operator()(KeyRec const& a, IndexKey const& bkey) const {
            int c = cmp_key(a.key, bkey);
            return c < 0 || (c==0 && a.recno < 0);
        }
        bool operator()(IndexKey const& akey, KeyRec const& b) const {
            int c = cmp_key(akey, b.key);
            return c < 0 || (c==0 && 0 < b.recno);
        }
    };
};

} // namespace xindex
