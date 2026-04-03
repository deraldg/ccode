// src/relations/rel_iter.cpp
#include "rel_iter.hpp"

#include "set_relations.hpp"     // relations_api
#include "xbase.hpp"

#include <cctype>
#include <string>
#include <vector>

using relations_api::RelationSpec;

namespace {

static inline std::string up(std::string s)
{
    for (auto& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static bool relation_defined(
    const std::vector<RelationSpec>& specs,
    const std::string& parent,
    const std::string& child)
{
    const std::string p = up(parent);
    const std::string c = up(child);

    for (const auto& r : specs) {
        if (up(r.parent) == p && up(r.child) == c)
            return true;
    }
    return false;
}

} // namespace

namespace rel_iter {

// ---------------------------------------------------------------------------
// preview_child
// ---------------------------------------------------------------------------

ChildResult preview_child(
    const std::string& parent_logical,
    const std::string& child_logical,
    int limit,
    int offset)
{
    (void)offset; // reserved for paging; not wired yet

    ChildResult out;
    out.child = child_logical;

    const std::string parent =
        parent_logical.empty()
            ? relations_api::current_parent_name()
            : parent_logical;

    out.parent = parent;

    if (parent.empty()) {
        out.status = Status::NO_PARENT_SET;
        return out;
    }

    // Verify relation exists
    const auto specs = relations_api::export_relations();
    if (!relation_defined(specs, parent, child_logical)) {
        out.status = Status::NO_RELATION_DEFINED;
        return out;
    }

    // Ask relations engine for match count
    int count = relations_api::match_count_for_child(child_logical);
    out.match_count = count;

    if (count < 0) {
        out.status = Status::RELATION_STALE;
        return out;
    }

    // Preview rows (relations_api already does tuple scanning)
    auto preview = relations_api::preview_child(child_logical, limit);

    out.rows.reserve(preview.size());
    for (const auto& p : preview) {
        out.rows.push_back({ p.line });
    }

    out.status = Status::OK;
    return out;
}

// ---------------------------------------------------------------------------
// preview_all_children
// ---------------------------------------------------------------------------

std::vector<ChildResult> preview_all_children(
    const std::string& parent_logical,
    int limit,
    int offset)
{
    (void)offset; // reserved for paging; not wired yet

    std::vector<ChildResult> results;

    const std::string parent =
        parent_logical.empty()
            ? relations_api::current_parent_name()
            : parent_logical;

    if (parent.empty())
        return results;

    const auto specs = relations_api::export_relations();

    for (const auto& r : specs) {
        if (up(r.parent) != up(parent))
            continue;

        results.push_back(
            preview_child(parent, r.child, limit, offset)
        );
    }

    return results;
}

} // namespace rel_iter
