#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "browse_args.hpp"

namespace xbase { class DbArea; }
namespace dottalk::browse { struct BrowseArgs; }

namespace dottalk::browse::order {

struct OrderView {
    std::vector<uint32_t> recnos; // 1-based
    bool asc = true;
    bool has_order = false;
};

OrderView build_order_view(::xbase::DbArea& area);

// Small helpers:
bool goto_recno(::xbase::DbArea& area, uint32_t rn);
void apply_start(::xbase::DbArea& area, BrowseArgs::StartPos start);
void apply_start_key_seek(::xbase::DbArea& area, const std::string& literal);

std::string summarize_order(::xbase::DbArea& area); // for banners

} // namespace dottalk::browse::order
