#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xbase {
class DbArea;
}

namespace cli {

enum class OrderContainerKind {
    None,   // physical DBF order
    Inx,    // legacy single-order index
    Cnx,    // CNX multi-tag container
    Cdx     // CDX / LMDB-backed compound index
};

enum class OrderDirection {
    Ascending,
    Descending
};

struct OrderSpec {
    OrderContainerKind kind = OrderContainerKind::None;
    std::string container_path;   // .inx / .cnx / .cdx path, or empty for physical
    std::string tag_name;         // optional for CNX/CDX
    OrderDirection direction = OrderDirection::Ascending;
    bool active = false;
};

struct OrderPosition {
    uint32_t recno = 0;
    bool eof = true;
};

struct OrderDiagnostic {
    bool ok = true;
    std::string message;
};

class IOrderIterator {
public:
    virtual ~IOrderIterator() = default;

    virtual OrderDiagnostic top() = 0;
    virtual OrderDiagnostic bottom() = 0;
    virtual OrderDiagnostic next() = 0;
    virtual OrderDiagnostic prev() = 0;

    virtual OrderPosition current() const = 0;
    virtual bool valid() const = 0;
};

class IOrderProvider {
public:
    virtual ~IOrderProvider() = default;

    // Create an iterator over the supplied order specification.
    virtual std::unique_ptr<IOrderIterator> createIterator(
        xbase::DbArea& area,
        const OrderSpec& spec
    ) = 0;

    // Materialize recnos in logical traversal order.
    // This is the easiest compatibility bridge for LIST / SMARTLIST / browser work.
    virtual OrderDiagnostic materializeRecnos(
        xbase::DbArea& area,
        const OrderSpec& spec,
        std::vector<uint32_t>& out_recnos
    ) = 0;

    // Convenience helpers for callers that just want a boundary record.
    virtual std::optional<uint32_t> firstRecno(
        xbase::DbArea& area,
        const OrderSpec& spec
    ) = 0;

    virtual std::optional<uint32_t> lastRecno(
        xbase::DbArea& area,
        const OrderSpec& spec
    ) = 0;
};

// Factory for the default DotTalk++ implementation.
std::unique_ptr<IOrderProvider> makeDefaultOrderProvider();

// Builds an OrderSpec from the current workspace / orderstate for this area.
OrderSpec buildActiveOrderSpec(xbase::DbArea& area);

} // namespace cli
