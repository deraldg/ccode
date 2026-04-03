// src/browser/browser_relation_adapter.cpp
#include "browser/browser_relation_adapter.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "../cli/set_relations.hpp"
#include "../cli/rel_enum_engine.hpp"

namespace browser
{
    namespace
    {
        static std::string upper_copy(const std::string& s)
        {
            std::string out = s;
            for (char& ch : out)
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            return out;
        }

        struct RawRelationEdge
        {
            std::string parent_alias;
            std::string child_alias;
            std::string on_expr;
        };

        static std::string join_fields_csv(const std::vector<std::string>& fields)
        {
            std::string out;
            for (std::size_t i = 0; i < fields.size(); ++i) {
                if (i) out += ",";
                out += fields[i];
            }
            return out;
        }

        static std::vector<RawRelationEdge> fetch_active_relation_edges()
        {
            std::vector<RawRelationEdge> out;

            const auto specs = relations_api::export_relations();
            out.reserve(specs.size());

            for (const auto& s : specs) {
                RawRelationEdge e{};
                e.parent_alias = s.parent;
                e.child_alias  = s.child;
                e.on_expr      = join_fields_csv(s.fields);
                out.push_back(std::move(e));
            }

            return out;
        }

        static std::vector<RawRelationEdge> children_of(
            const std::vector<RawRelationEdge>& edges,
            const std::string& parent_alias)
        {
            std::vector<RawRelationEdge> out;
            const std::string up_parent = upper_copy(parent_alias);

            for (const auto& e : edges) {
                if (upper_copy(e.parent_alias) == up_parent)
                    out.push_back(e);
            }

            return out;
        }

        static RelationNode build_relation_node_recursive(
            const RawRelationEdge& edge,
            const std::vector<RawRelationEdge>& edges,
            std::set<std::pair<std::string, std::string>>& seen_edges)
        {
            RelationNode node{};
            node.parent_alias = edge.parent_alias;
            node.child_alias  = edge.child_alias;
            node.on_expr      = edge.on_expr;

            const auto edge_key = std::make_pair(
                upper_copy(edge.parent_alias),
                upper_copy(edge.child_alias));

            if (seen_edges.count(edge_key))
                return node;

            seen_edges.insert(edge_key);

            const auto kids = children_of(edges, edge.child_alias);
            node.children.reserve(kids.size());

            for (const auto& child_edge : kids)
                node.children.push_back(build_relation_node_recursive(child_edge, edges, seen_edges));

            seen_edges.erase(edge_key);
            return node;
        }

        static std::vector<RelationNode> build_root_links(
            const std::string& root_alias,
            const std::vector<RawRelationEdge>& edges)
        {
            std::vector<RelationNode> out;
            const auto root_children = children_of(edges, root_alias);
            out.reserve(root_children.size());

            for (const auto& edge : root_children) {
                std::set<std::pair<std::string, std::string>> seen_edges;
                out.push_back(build_relation_node_recursive(edge, edges, seen_edges));
            }

            return out;
        }
    } // namespace

    bool relation_build_tree(const std::string& root_alias,
                             RelationTreeSnapshot& out_tree,
                             std::vector<std::string>& warnings,
                             std::string& status)
    {
        out_tree = RelationTreeSnapshot{};
        out_tree.root_alias = root_alias;

        if (root_alias.empty()) {
            warnings.push_back("relation_build_tree: empty root alias.");
            status = "ERROR";
            return false;
        }

        const auto edges = fetch_active_relation_edges();
        out_tree.links = build_root_links(root_alias, edges);

        if (edges.empty()) {
            if (status.empty()) status = "OK";
            return true;
        }

        if (out_tree.links.empty()) {
            warnings.push_back("No active relations found from root alias " + root_alias + ".");
            if (status == "OK") status = "WARN";
        }

        return true;
    }

    bool relation_enumerate(xbase::DbArea& root_area,
                            const EnumRequest& req,
                            EnumResult& out)
    {
        out = EnumResult{};

        rel_enum_engine::Request eng{};
        eng.root_alias = req.root_alias;
        eng.path_aliases = req.path_aliases;
        eng.limit = (req.limit < 0) ? 0u : static_cast<std::size_t>(req.limit);
        eng.distinct = false;

        eng.tuple_exprs.reserve(req.columns.size());
        for (const auto& c : req.columns)
            eng.tuple_exprs.push_back(c.expr);

        rel_enum_engine::Result res{};
        if (!rel_enum_engine::run(root_area, eng, res)) {
            out.warnings = std::move(res.warnings);
            out.status = res.status.empty() ? "ERROR" : res.status;
            return false;
        }

        out.status = res.status;
        out.warnings = std::move(res.warnings);

        out.rows.reserve(res.rows.size());
        for (auto& r : res.rows) {
            TupleRow tr{};
            tr.cells = std::move(r.cells);
            out.rows.push_back(std::move(tr));
        }

        out.counts.reserve(res.counts.size());
        for (auto& c : res.counts) {
            DescendantCount dc{};
            dc.alias = c.alias;
            dc.count = c.count;
            out.counts.push_back(std::move(dc));
        }

        return true;
    }
}