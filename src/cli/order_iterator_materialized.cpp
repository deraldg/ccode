#include "cli/order_iterator_materialized.hpp"

#include <utility>

namespace cli {

MaterializedOrderIterator::MaterializedOrderIterator(std::vector<uint32_t> recnos)
    : recnos_(std::move(recnos))
{
    if (!recnos_.empty()) {
        pos_ = 0;
        valid_ = true;
    }
}

OrderDiagnostic MaterializedOrderIterator::top()
{
    if (recnos_.empty()) {
        valid_ = false;
        return {true, ""};
    }

    pos_ = 0;
    valid_ = true;
    return {true, ""};
}

OrderDiagnostic MaterializedOrderIterator::bottom()
{
    if (recnos_.empty()) {
        valid_ = false;
        return {true, ""};
    }

    pos_ = recnos_.size() - 1;
    valid_ = true;
    return {true, ""};
}

OrderDiagnostic MaterializedOrderIterator::next()
{
    if (!valid_) {
        return {true, ""};
    }

    if (pos_ + 1 >= recnos_.size()) {
        valid_ = false;
        return {true, ""};
    }

    ++pos_;
    return {true, ""};
}

OrderDiagnostic MaterializedOrderIterator::prev()
{
    if (!valid_) {
        return {true, ""};
    }

    if (pos_ == 0) {
        valid_ = false;
        return {true, ""};
    }

    --pos_;
    return {true, ""};
}

OrderPosition MaterializedOrderIterator::current() const
{
    if (!valid_ || recnos_.empty()) {
        return {0, true};
    }

    return {recnos_[pos_], false};
}

bool MaterializedOrderIterator::valid() const
{
    return valid_;
}

} // namespace cli
