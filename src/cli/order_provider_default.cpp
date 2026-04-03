#include "cli/order_provider_default.hpp"
#include "cli/order_iterator_materialized.hpp"

#include "cli/order_state.hpp"
#include "xbase.hpp"

// IMPORTANT:
// If your INX loader lives in a different header, change this include to match.
// The only place that should know about it is this provider implementation.
#include "cli/order_nav.hpp"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace cli {

using xbase::DbArea;

namespace {

// This tiny helper keeps repo-specific details localized here.
static OrderDirection get_direction(DbArea& area)
{
    return orderstate::isAscending(area)
        ? OrderDirection::Ascending
        : OrderDirection::Descending;
}

} // namespace

std::unique_ptr<IOrderProvider> makeDefaultOrderProvider()
{
    return std::make_unique<DefaultOrderProvider>();
}

OrderSpec buildActiveOrderSpec(DbArea& area)
{
    OrderSpec spec{};
    spec.direction = get_direction(area);

    if (!orderstate::hasOrder(area)) {
        spec.kind = OrderContainerKind::None;
        spec.active = false;
        return spec;
    }

    spec.active = true;
    spec.container_path = orderstate::orderName(area);
    spec.tag_name = orderstate::activeTag(area);

    if (orderstate::isCnx(area)) {
        spec.kind = OrderContainerKind::Cnx;
    } else if (orderstate::isCdx(area)) {
        spec.kind = OrderContainerKind::Cdx;
    } else {
        spec.kind = OrderContainerKind::Inx;
    }

    return spec;
}

std::unique_ptr<IOrderIterator> DefaultOrderProvider::createIterator(
    DbArea& area,
    const OrderSpec& spec
)
{
    std::vector<uint32_t> recnos;
    const OrderDiagnostic d = materializeRecnos(area, spec, recnos);
    if (!d.ok) {
        return nullptr;
    }

    return std::make_unique<MaterializedOrderIterator>(std::move(recnos));
}

OrderDiagnostic DefaultOrderProvider::materializeRecnos(
    DbArea& area,
    const OrderSpec& spec,
    std::vector<uint32_t>& out_recnos
)
{
    out_recnos.clear();

    switch (spec.kind) {
    case OrderContainerKind::None:
        return materializePhysical(area, spec, out_recnos);

    case OrderContainerKind::Inx:
        return materializeInx(area, spec, out_recnos);

    case OrderContainerKind::Cnx:
        return materializeCnx(area, spec, out_recnos);

    case OrderContainerKind::Cdx:
        return materializeCdx(area, spec, out_recnos);
    }

    return {false, "Unknown order container kind."};
}

std::optional<uint32_t> DefaultOrderProvider::firstRecno(
    DbArea& area,
    const OrderSpec& spec
)
{
    std::vector<uint32_t> recnos;
    const OrderDiagnostic d = materializeRecnos(area, spec, recnos);
    if (!d.ok || recnos.empty()) {
        return std::nullopt;
    }

    return recnos.front();
}

std::optional<uint32_t> DefaultOrderProvider::lastRecno(
    DbArea& area,
    const OrderSpec& spec
)
{
    std::vector<uint32_t> recnos;
    const OrderDiagnostic d = materializeRecnos(area, spec, recnos);
    if (!d.ok || recnos.empty()) {
        return std::nullopt;
    }

    return recnos.back();
}

OrderDiagnostic DefaultOrderProvider::materializePhysical(
    DbArea& area,
    const OrderSpec& spec,
    std::vector<uint32_t>& out_recnos
)
{
    out_recnos.clear();

    const uint32_t n = static_cast<uint32_t>(area.recCount());
    out_recnos.reserve(n);

    for (uint32_t rec = 1; rec <= n; ++rec) {
        out_recnos.push_back(rec);
    }

    reverseIfDescending(spec, out_recnos);
    return {true, ""};
}

OrderDiagnostic DefaultOrderProvider::materializeInx(
    DbArea& area,
    const OrderSpec& spec,
    std::vector<uint32_t>& out_recnos
)
{
    out_recnos.clear();

    // This call is intentionally centralized here so commands never touch it.
    // Adjust the namespace/function name only here if needed.
    if (!order_nav_detail::load_inx_recnos(
            spec.container_path,
            static_cast<uint32_t>(area.recCount()),
            out_recnos)) {
        return {false, "Failed to read INX order: " + spec.container_path};
    }

    reverseIfDescending(spec, out_recnos);
    return {true, ""};
}

OrderDiagnostic DefaultOrderProvider::materializeCnx(
    DbArea& area,
    const OrderSpec& spec,
    std::vector<uint32_t>& out_recnos
)
{
    out_recnos.clear();

    // PHASE 1 CNX STRATEGY:
    // Keep this intentionally conservative.
    //
    // If you already have a CNX recno materializer, wire it here.
    // Otherwise this safe fallback gives commands a stable surface now.
    //
    // Replace later with real CNX tag-aware traversal without changing callers.

    const uint32_t n = static_cast<uint32_t>(area.recCount());
    out_recnos.reserve(n);

    for (uint32_t rec = 1; rec <= n; ++rec) {
        out_recnos.push_back(rec);
    }

    reverseIfDescending(spec, out_recnos);
    return {true, ""};
}

OrderDiagnostic DefaultOrderProvider::materializeCdx(
    DbArea& area,
    const OrderSpec& spec,
    std::vector<uint32_t>& out_recnos
)
{
    out_recnos.clear();

    // PHASE 1 CDX/LMDB STRATEGY:
    // This is the seam you replace later with true CDX/LMDB traversal,
    // persisted RUN1 order loading, or an LMDB cursor-backed materializer.
    //
    // For now, keep the command surface stable.

    const uint32_t n = static_cast<uint32_t>(area.recCount());
    out_recnos.reserve(n);

    for (uint32_t rec = 1; rec <= n; ++rec) {
        out_recnos.push_back(rec);
    }

    reverseIfDescending(spec, out_recnos);
    return {true, ""};
}

void DefaultOrderProvider::reverseIfDescending(
    const OrderSpec& spec,
    std::vector<uint32_t>& recnos
)
{
    if (spec.direction == OrderDirection::Descending) {
        std::reverse(recnos.begin(), recnos.end());
    }
}

} // namespace cli
