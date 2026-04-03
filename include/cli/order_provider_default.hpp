#pragma once

#include "cli/order_provider.hpp"

namespace cli {

class DefaultOrderProvider final : public IOrderProvider {
public:
    DefaultOrderProvider() = default;
    ~DefaultOrderProvider() override = default;

    std::unique_ptr<IOrderIterator> createIterator(
        xbase::DbArea& area,
        const OrderSpec& spec
    ) override;

    OrderDiagnostic materializeRecnos(
        xbase::DbArea& area,
        const OrderSpec& spec,
        std::vector<uint32_t>& out_recnos
    ) override;

    std::optional<uint32_t> firstRecno(
        xbase::DbArea& area,
        const OrderSpec& spec
    ) override;

    std::optional<uint32_t> lastRecno(
        xbase::DbArea& area,
        const OrderSpec& spec
    ) override;

private:
    OrderDiagnostic materializePhysical(
        xbase::DbArea& area,
        const OrderSpec& spec,
        std::vector<uint32_t>& out_recnos
    );

    OrderDiagnostic materializeInx(
        xbase::DbArea& area,
        const OrderSpec& spec,
        std::vector<uint32_t>& out_recnos
    );

    OrderDiagnostic materializeCnx(
        xbase::DbArea& area,
        const OrderSpec& spec,
        std::vector<uint32_t>& out_recnos
    );

    OrderDiagnostic materializeCdx(
        xbase::DbArea& area,
        const OrderSpec& spec,
        std::vector<uint32_t>& out_recnos
    );

    static void reverseIfDescending(
        const OrderSpec& spec,
        std::vector<uint32_t>& recnos
    );
};

} // namespace cli
