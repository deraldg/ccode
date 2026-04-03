#pragma once
#include <string>
#include <vector>

namespace relations_status {

struct ChildStat {
    std::string child_area;  // logical child area
    int         match_count; // count for current parent (zero-scan)
};

std::string relation_status_token();
std::vector<ChildStat> relation_stats_for_current_parent();

} // namespace relations_status
