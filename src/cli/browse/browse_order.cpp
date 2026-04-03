#include "browse_order.hpp"
#include "browse_args.hpp"
#include "xbase.hpp"

// If you have orderstate+order_nav available, include them here when migrating.
// #include "cli/order_state.hpp"
// #include "cli/order_nav.hpp"
// #include "xindex/order_display.hpp"

namespace dottalk::browse::order {

OrderView build_order_view(::xbase::DbArea& /*area*/){
    // Skeleton: migrate CNX/INX collection here.
    OrderView v; v.asc = true; v.has_order = false;
    return v;
}

bool goto_recno(::xbase::DbArea& area, uint32_t rn){
    return area.gotoRec((int32_t)rn) && area.readCurrent();
}

void apply_start(::xbase::DbArea& area, BrowseArgs::StartPos start){
    if (start == BrowseArgs::StartPos::Top) area.top(); else area.bottom();
}

void apply_start_key_seek(::xbase::DbArea& /*area*/, const std::string& /*literal*/){
    // Skeleton: call cmd_SEEK once migrated.
}

std::string summarize_order(::xbase::DbArea& /*area*/){
    // Skeleton: replace with orderdisplay::summarize(area);
    return "(physical order)";
}

} // namespace dottalk::browse::order
