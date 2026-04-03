// src/cli/set_relations.hpp
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace xbase { class XBaseEngine; }

namespace relations_api {

// Engine wiring (optional; used by some tooling)
void attach_engine(xbase::XBaseEngine* eng) noexcept;

// Runtime knobs
void set_autorefresh(bool on) noexcept;
void set_verbose(bool on) noexcept;
void set_scan_limit(std::size_t max_steps) noexcept;

// Relation graph editing
bool add_relation(const std::string& parent_area,
                  const std::string& child_area,
                  const std::vector<std::string>& tuple_fields);

bool remove_relation(const std::string& parent_area,
                     const std::string& child_area);

void clear_relations(const std::string& parent_area);
void clear_all_relations();

// Parent “anchor” (the starting parent for traversal when refreshing)
void set_current_parent_name(const std::string& logical_name) noexcept;
std::string current_parent_name();

// Traversal / refresh
void refresh_for_current_parent() noexcept;
void refresh_if_enabled() noexcept;

/**
 * Enumerate the INNER-JOIN rows implied by the relation chain rooted at the current parent record.
 *
 * This temporarily repositions child areas while emitting. The original record pointers are restored
 * before returning (best-effort).
 *
 * @param path_children Optional explicit chain of child logical names. If empty, a unique chain is inferred
 *                      by repeatedly following the only child relation at each step.
 * @param max_rows      Max rows to emit (0 = no limit).
 * @param emit          Callback invoked once per join row after all involved areas are positioned.
 * @param rows_emitted  Optional out param for the number of emitted rows.
 * @return true on success (even if 0 rows), false on error.
 */
bool join_emit_for_current_parent(const std::vector<std::string>& path_children,
                                  std::size_t max_rows,
                                  const std::function<void()>& emit,
                                  std::size_t* rows_emitted = nullptr);

/**
 * Emit exactly ONE join row for the CURRENT parent row using the currently-resolved relation context.
 *
 * This preserves the historical REL JOIN behavior (single-row "current pointers" output).
 * Use `REL JOIN ONE ...` at the CLI to force this behavior.
 *
 * @param path_children Optional chain (currently ignored; reserved for future).
 * @param max_rows      Max rows to emit (0 = no limit; ONE-mode still emits at most 1).
 * @param emit          Callback invoked once (when allowed by max_rows).
 * @param rows_emitted  Optional out param for the number of emitted rows (0 or 1).
 * @return true on success (even if 0 rows), false on error.
 */
bool join_emit_one_for_current_parent(const std::vector<std::string>& path_children,
                                      std::size_t max_rows,
                                      const std::function<void()>& emit,
                                      std::size_t* rows_emitted = nullptr);


// Enumerate ALL join rows for the CURRENT parent row.
//
// This is the engine behind REL ENUM (and the default REL JOIN behavior).
//
// Semantics:
// - path_children may be empty; in that case a unique single-child chain is
//   inferred by repeatedly following the only child relation at each step.
// - max_rows: 0 means unlimited.
// - emit() is called once per full match with all involved areas positioned.
bool enum_emit_for_current_parent(const std::vector<std::string>& path_children,
                                  std::size_t max_rows,
                                  const std::function<void()>& emit,
                                  std::size_t* rows_emitted = nullptr);

// Introspection helpers
std::vector<std::string> child_areas_for_current_parent();
int match_count_for_child(const std::string& child_area);

// Persistence interchange
struct RelationSpec {
    std::string parent;
    std::string child;
    std::vector<std::string> fields;
};

std::vector<RelationSpec> export_relations();
void import_relations(const std::vector<RelationSpec>& specs, bool clear_existing);

// Debug / UI
struct PreviewRow { std::string line; };
// Tree view for REL LIST ALL
std::vector<PreviewRow> list_tree_for_current_parent(bool recursive, int max_depth);

std::vector<PreviewRow> preview_child(const std::string& child_area, int limit);

} // namespace relations_api