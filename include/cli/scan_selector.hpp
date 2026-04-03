#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xbase { class DbArea; }

namespace cli::scan {

enum class ScanMode {
    Current,
    All,
    Rest,
    NextN
};

enum class DeletedMode {
    UseDefault,
    OnlyDeleted,
    OnlyAlive
};

struct SelectionSpec {
    ScanMode scan_mode{ScanMode::All};
    int next_n{0};

    DeletedMode deleted_mode{DeletedMode::UseDefault};

    bool use_expr{false};
    std::string expr;

    bool ordered_snapshot{true};
};

struct SelectionResult {
    std::vector<uint64_t> recnos;
    std::string warning;
};

// Match the current row only.
// Honors SET FILTER, deleted policy, and optional expr.
bool match_current(xbase::DbArea& area,
                   const SelectionSpec& spec);

// Collect selected recnos according to the spec.
SelectionResult collect_selected_recnos(xbase::DbArea& area,
                                        const SelectionSpec& spec);

} // namespace cli::scan