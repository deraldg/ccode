// === FILE: include/cli/order_controller.hpp ===
#pragma once

#include <string>

namespace xbase { class DbArea; }

namespace cli {

// Thin OO façade over the existing orderstate functions.
// Step 1 uses orderstate under the hood; later we can migrate storage here.
class OrderController {
public:
    explicit OrderController(xbase::DbArea& area);

    bool        isAscending() const;           // current direction
    void        setAscending(bool asc);        // set ASC/DESC
    std::string orderName()  const;            // may be a full path to .inx
    void        setOrder(const std::string& indexPathOrTag);

private:
    xbase::DbArea& area_;
};

} // namespace cli


// === FILE: src/cli/order_controller.cpp ===
#include "cli/order_controller.hpp"
#include "order_state.hpp"
#include "xbase.hpp"

namespace cli {

OrderController::OrderController(xbase::DbArea& area) : area_(area) {}

bool OrderController::isAscending() const {
    return orderstate::isAscending(area_);
}

void OrderController::setAscending(bool asc) {
    if (asc) orderstate::setAscending(area_); else orderstate::setDescending(area_);
}

std::string OrderController::orderName() const {
    return orderstate::orderName(area_);
}

void OrderController::setOrder(const std::string& indexPathOrTag) {
    orderstate::setOrder(area_, indexPathOrTag);
}

} // namespace cli


// === FILE: include/cli/table_context.hpp ===
#pragma once

#include <memory>

namespace xbase { class DbArea; }
namespace cli { class OrderController; }

namespace cli {

// Per-table context that owns controllers associated with a DbArea.
class TableContext {
public:
    explicit TableContext(xbase::DbArea& area);

    xbase::DbArea&   area()  { return area_; }
    OrderController& order() { return *order_; }

private:
    xbase::DbArea& area_;
    std::unique_ptr<OrderController> order_;
};

// Registry: obtain (or create) a context for a given DbArea.
TableContext& ensure_table(xbase::DbArea& area);

} // namespace cli


// === FILE: src/cli/table_context.cpp ===
#include "cli/table_context.hpp"
#include "cli/order_controller.hpp"
#include "xbase.hpp"

#include <unordered_map>
#include <memory>

namespace cli {

TableContext::TableContext(xbase::DbArea& area)
    : area_(area), order_(std::make_unique<OrderController>(area)) {}

static std::unordered_map<xbase::DbArea*, std::unique_ptr<TableContext>>& registry() {
    static std::unordered_map<xbase::DbArea*, std::unique_ptr<TableContext>> g;
    return g;
}

TableContext& ensure_table(xbase::DbArea& area) {
    auto& r = registry();
    auto it = r.find(&area);
    if (it != r.end()) return *it->second;
    auto ctx = std::make_unique<TableContext>(area);
    auto* raw = ctx.get();
    r.emplace(&area, std::move(ctx));
    return *raw;
}

} // namespace cli
