// src/browser/browser_render_console.cpp
#include "browser/browser_render_console.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace browser
{
    namespace
    {
        static int clamp_console_width(int w)
        {
            if (w < 1)  return 1;
            if (w > 20) return 20;
            return w;
        }

        static bool is_right_aligned(const TupleColumnMeta& col)
        {
            return col.type.is_numeric;
        }

        static std::string pad_left(const std::string& s, int width)
        {
            if (width <= 0) return "";
            if (static_cast<int>(s.size()) >= width) return s;
            return std::string(static_cast<size_t>(width - static_cast<int>(s.size())), ' ') + s;
        }

        static std::string pad_right(const std::string& s, int width)
        {
            if (width <= 0) return "";
            if (static_cast<int>(s.size()) >= width) return s;
            return s + std::string(static_cast<size_t>(width - static_cast<int>(s.size())), ' ');
        }

        static std::string join_path(const std::vector<std::string>& parts, const std::string& sep)
        {
            if (parts.empty()) return "(none)";

            std::ostringstream oss;
            for (size_t i = 0; i < parts.size(); ++i)
            {
                if (i) oss << sep;
                oss << parts[i];
            }
            return oss.str();
        }

        static std::string format_cell(const std::string& value,
                                       const TupleColumnMeta& col,
                                       int width)
        {
            if (width <= 0) return "";

            std::string s = value;
            if (static_cast<int>(s.size()) > width)
                s.resize(static_cast<size_t>(width));

            return is_right_aligned(col) ? pad_left(s, width)
                                         : pad_right(s, width);
        }

        static std::vector<std::string> build_visible_labels(const std::vector<TupleColumnMeta>& cols)
        {
            std::map<std::string, int> counts;
            for (const auto& c : cols)
                ++counts[c.label];

            std::vector<std::string> out;
            out.reserve(cols.size());

            for (const auto& c : cols)
            {
                const std::string base = c.label.empty() ? c.expr : c.label;
                if (counts[c.label] > 1 && !c.source_alias.empty())
                    out.push_back(c.source_alias + "." + base);
                else
                    out.push_back(base);
            }

            return out;
        }

        static std::vector<int> compute_display_widths(const std::vector<TupleColumnMeta>& cols,
                                                       const std::vector<TupleRow>& rows,
                                                       const std::vector<std::string>& labels)
        {
            std::vector<int> widths(cols.size(), 1);

            for (size_t i = 0; i < cols.size(); ++i)
            {
                int w = cols[i].width;
                w = std::max(w, static_cast<int>(labels[i].size()));

                for (const auto& row : rows)
                {
                    if (i < row.cells.size())
                        w = std::max(w, static_cast<int>(row.cells[i].size()));
                }

                widths[i] = clamp_console_width(w);
            }

            return widths;
        }

        static void render_header(const BrowserSnapshot& snap)
        {
            std::cout << snap.command_name << " Relational Browser\n";
            std::cout << "ROOT: "
                      << (snap.root.alias.empty() ? "(none)" : snap.root.alias)
                      << "  RECNO: " << snap.root.recno << "\n";
            std::cout << "PATH: " << join_path(snap.path_aliases, " -> ") << "\n";
            std::cout << "LIMIT: " << snap.limit << "\n";

            if (snap.order.has_order)
            {
                std::cout << "ORDER: "
                          << (snap.order.container_path.empty() ? "(container?)" : snap.order.container_path);

                if (!snap.order.tag_name.empty())
                    std::cout << "  TAG " << snap.order.tag_name;

                if (!snap.order.direction.empty())
                    std::cout << "  " << snap.order.direction;

                if (snap.order.physical_fallback)
                    std::cout << "  [fallback]";

                std::cout << "\n";
            }
            else
            {
                std::cout << "ORDER: physical\n";
            }

            std::cout << "\n";
        }

        static void render_warnings(const std::vector<std::string>& warnings)
        {
            if (warnings.empty())
                return;

            std::cout << "WARNINGS\n";
            for (const auto& w : warnings)
                std::cout << "  - " << w << "\n";
            std::cout << "\n";
        }

        static void render_root(const RecordSnapshot& root)
        {
            std::cout << "CURRENT ROOT RECORD\n";

            if (root.fields.empty() && root.values.empty())
            {
                std::cout << "  (none)\n\n";
                return;
            }

            if (root.fields.size() != root.values.size())
            {
                std::cout << "  (field/value mismatch)\n\n";
                return;
            }

            size_t max_name = 0;
            for (const auto& f : root.fields)
                max_name = std::max(max_name, f.name.size());

            for (size_t i = 0; i < root.fields.size(); ++i)
            {
                std::cout << "  "
                          << pad_right(root.fields[i].name, static_cast<int>(max_name))
                          << " : "
                          << root.values[i]
                          << "\n";
            }

            std::cout << "\n";
        }

        static void render_relation_node(const RelationNode& node, int depth)
        {
            std::string indent(static_cast<size_t>(depth * 3), ' ');
            std::cout << indent << "-> " << node.child_alias;

            if (!node.on_expr.empty())
                std::cout << "   ON " << node.on_expr;

            std::cout << "\n";

            for (const auto& child : node.children)
                render_relation_node(child, depth + 1);
        }

        static void render_relation_tree(const RelationTreeSnapshot& tree)
        {
            std::cout << "RELATION TREE\n";

            if (tree.root_alias.empty() && tree.links.empty())
            {
                std::cout << "  (none)\n\n";
                return;
            }

            std::cout << "  " << tree.root_alias << "\n";
            if (tree.links.empty())
            {
                std::cout << "  (none)\n\n";
                return;
            }

            for (const auto& link : tree.links)
                render_relation_node(link, 1);

            std::cout << "\n";
        }

        static void render_descendant_summary(const DescendantSummary& ds)
        {
            std::cout << "DESCENDANT SUMMARY\n";

            if (ds.counts.empty())
            {
                std::cout << "  (none)\n\n";
                return;
            }

            size_t max_alias = 0;
            for (const auto& c : ds.counts)
                max_alias = std::max(max_alias, c.alias.size());

            for (const auto& c : ds.counts)
            {
                std::cout << "  "
                          << pad_right(c.alias, static_cast<int>(max_alias))
                          << " : "
                          << c.count
                          << "\n";
            }

            std::cout << "\n";
        }

        static void render_tuple_grid(const std::vector<TupleColumnMeta>& cols,
                                      const std::vector<TupleRow>& rows)
        {
            std::cout << "TUPLE GRID\n";

            if (cols.empty())
            {
                std::cout << "  (none)\n\n";
                return;
            }

            const auto labels = build_visible_labels(cols);
            const auto widths = compute_display_widths(cols, rows, labels);

            for (size_t i = 0; i < cols.size(); ++i)
            {
                if (i) std::cout << ' ';
                std::cout << pad_right(labels[i], widths[i]);
            }
            std::cout << "\n";

            for (const auto& row : rows)
            {
                if (row.cells.size() != cols.size())
                {
                    std::cout << "(row/column mismatch)\n";
                    continue;
                }

                for (size_t i = 0; i < cols.size(); ++i)
                {
                    if (i) std::cout << ' ';
                    std::cout << format_cell(row.cells[i], cols[i], widths[i]);
                }
                std::cout << "\n";
            }

            std::cout << "\n";
        }

        static void render_footer(const BrowserSnapshot& snap)
        {
            std::cout << "ROWS SHOWN: " << snap.rows_shown
                      << " / LIMIT " << snap.limit
                      << " | STATUS: " << snap.status
                      << "\n";
        }
    } // namespace

    void render_browser_snapshot_console(const BrowserSnapshot& snap)
    {
        render_header(snap);
        render_warnings(snap.warnings);
        render_root(snap.root);
        render_relation_tree(snap.relation_tree);
        render_descendant_summary(snap.descendant_summary);
        render_tuple_grid(snap.columns, snap.rows);
        render_footer(snap);
    }

    void render_walk_snapshot_console(const WalkSnapshot& walk)
    {
        std::cout << "ERSATZ WALK\n\n";

        if (walk.frames.empty())
        {
            std::cout << "  (none)\n";
            if (!walk.warnings.empty())
            {
                std::cout << "\nWARNINGS\n";
                for (const auto& w : walk.warnings)
                    std::cout << "  - " << w << "\n";
            }
            return;
        }

        for (size_t i = 0; i < walk.frames.size(); ++i)
        {
            std::cout << "==== FRAME " << (i + 1) << " ====\n";
            render_browser_snapshot_console(walk.frames[i]);
            std::cout << "\n";
        }

        if (!walk.warnings.empty())
        {
            std::cout << "WALK WARNINGS\n";
            for (const auto& w : walk.warnings)
                std::cout << "  - " << w << "\n";
        }
    }
}
