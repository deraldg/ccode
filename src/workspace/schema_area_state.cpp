#include "workspace/schema_area_state.hpp"

#include "workspace/workarea_slot.hpp"
#include "xbase.hpp"

namespace dottalk::workspace
{

bool SchemaAreaState::is_valid(std::string* err) const
{
    if (slot < 0)
    {
        if (err) *err = "SchemaAreaState: slot must be >= 0";
        return false;
    }

    if (is_open && dbf_path.empty())
    {
        if (err) *err = "SchemaAreaState: open area missing dbf_path";
        return false;
    }

    return true;
}

bool SchemaAreaState::capture_from_dbarea(
    int slot_num,
    const xbase::DbArea& area,
    std::string* err)
{
    slot = slot_num;
    is_open = area.isOpen();

    if (!is_open)
    {
        return true;
    }

    dbf_path     = area.filename();
    logical_name = area.logicalName();
    alias        = area.logicalName();
    recno        = area.recno();

    memo_path = area.memoPath();

    switch (area.memoKind())
    {
    case xbase::DbArea::MemoKind::NONE:
        memo_kind = "none";
        break;

    case xbase::DbArea::MemoKind::FPT:
        memo_kind = "fpt";
        break;

    case xbase::DbArea::MemoKind::DBT:
        memo_kind = "dbt";
        break;

    default:
        memo_kind = "unknown";
        break;
    }

    is_x64 = (area.versionByte() == 0x64);

    // Deferred until stable accessors are chosen.
    index_container_path.clear();
    active_tag.clear();
    active_order_name.clear();

    return is_valid(err);
}

bool SchemaAreaState::apply_to_slot(
    WorkAreaSlot& slot_ref,
    std::string* err) const
{
    if (!is_valid(err))
    {
        return false;
    }

    slot_ref.set_alias(alias);

    if (!is_open)
    {
        return slot_ref.close_table(err);
    }

    if (!slot_ref.open_table(dbf_path, err))
    {
        return false;
    }

    xbase::DbArea* area = slot_ref.dbarea();
    if (!area)
    {
        if (err) *err = "apply_to_slot: slot opened but DbArea is null";
        return false;
    }

    if (recno > 0)
    {
        (void)area->gotoRec(recno);
    }

    // Active tag/order restoration deferred to a later pass.

    return true;
}

} // namespace dottalk::workspace
