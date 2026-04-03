#pragma once

#include "cli/order_provider.hpp"

#include <cstddef>
#include <vector>

namespace cli {

class MaterializedOrderIterator final : public IOrderIterator {
public:
    MaterializedOrderIterator() = default;
    explicit MaterializedOrderIterator(std::vector<uint32_t> recnos);

    OrderDiagnostic top() override;
    OrderDiagnostic bottom() override;
    OrderDiagnostic next() override;
    OrderDiagnostic prev() override;

    OrderPosition current() const override;
    bool valid() const override;

private:
    std::vector<uint32_t> recnos_;
    std::size_t pos_ = 0;
    bool valid_ = false;
};

} // namespace cli
