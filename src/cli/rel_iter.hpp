// src/relations/rel_iter.hpp
#pragma once

#include <string>
#include <vector>

namespace rel_iter {

// ---------------- Status ----------------------------------------------------

enum class Status {
    OK,
    NO_PARENT_SET,
    PARENT_NOT_OPEN,
    CHILD_NOT_OPEN,
    NO_RELATION_DEFINED,
    RELATION_STALE,
    ERROR
};

// ---------------- Result Types ----------------------------------------------

struct TupleRow {
    std::string line;
};

struct ChildResult {
    std::string parent;
    std::string child;
    int         match_count{0};
    Status      status{Status::ERROR};
    std::vector<TupleRow> rows;
};

// ---------------- API --------------------------------------------------------

/*
 * Preview rows for a single child of a parent.
 *
 * parent_logical:
 *   - empty => use relations_api::current_parent_name()
 *
 * child_logical:
 *   - required
 *
 * limit:
 *   - maximum number of rows to return
 *
 * offset:
 *   - starting offset into matches (optional paging)
 */
ChildResult preview_child(
    const std::string& parent_logical,
    const std::string& child_logical,
    int limit,
    int offset = 0
);

/*
 * Preview all children for a parent.
 * Returns one ChildResult per related child.
 */
std::vector<ChildResult> preview_all_children(
    const std::string& parent_logical,
    int limit,
    int offset = 0
);

} // namespace rel_iter
