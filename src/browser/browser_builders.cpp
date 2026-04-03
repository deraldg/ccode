// src/browser/browser_builders.cpp
#include "browser/browser_builders.hpp"

#include "browser/browser_relation_adapter.hpp"
#include "../cli/workareas.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

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

        static std::string area_alias_like(const xbase::DbArea& area)
        {
            try
            {
                const std::string logical = area.logicalName();
                if (!logical.empty()) return logical;
            }
            catch (...) {}

            try
            {
                const std::string legacy = area.name();
                if (!legacy.empty()) return legacy;
            }
            catch (...) {}

            try
            {
                const std::string base = area.dbfBasename();
                if (!base.empty()) return base;
            }
            catch (...) {}

            try
            {
                const std::string file = area.filename();
                if (!file.empty()) return file;
            }
            catch (...) {}

            return {};
        }

        static TypeMeta fallback_type_meta_from_code(char type_code)
        {
            TypeMeta tm{};
            const char t = static_cast<char>(std::toupper(static_cast<unsigned char>(type_code)));

            switch (t)
            {
            case 'C':
                tm.code = "C"; tm.name = "Character"; tm.is_text = true; break;
            case 'N':
                tm.code = "N"; tm.name = "Numeric"; tm.is_numeric = true; break;
            case 'F':
                tm.code = "F"; tm.name = "Float"; tm.is_numeric = true; break;
            case 'D':
                tm.code = "D"; tm.name = "Date"; tm.is_date = true; break;
            case 'L':
                tm.code = "L"; tm.name = "Logical"; tm.is_logical = true; break;
            case 'M':
                tm.code = "M"; tm.name = "Memo"; tm.is_memo = true; break;
            case 'I':
                tm.code = "I"; tm.name = "Integer"; tm.is_numeric = true; break;
            case 'Y':
                tm.code = "Y"; tm.name = "Currency"; tm.is_numeric = true; break;
            case 'T':
                tm.code = "T"; tm.name = "DateTime"; tm.is_date = true; break;
            case 'B':
                tm.code = "B"; tm.name = "Double"; tm.is_numeric = true; break;
            default:
                tm.code = std::string(1, t ? t : 'C');
                tm.name = "Character";
                tm.is_text = true;
                break;
            }

            return tm;
        }

        static int browser_default_width(const TypeMeta& tm, int declared_length, int decimals)
        {
            if (declared_length > 0)
                return declared_length;

            if (tm.is_memo)    return 10;
            if (tm.is_date)    return (tm.code == "T") ? 16 : 8;
            if (tm.is_logical) return 1;
            if (tm.is_numeric) return (decimals > 0) ? 12 : 10;
            return 12;
        }

        static int score_default_field_name(const std::string& upper_name,
                                            const TypeMeta& tm,
                                            bool prefer_identity)
        {
            if (upper_name.empty()) return -100;
            if (tm.is_memo) return -100;

            int score = 0;

            if (prefer_identity)
            {
                if (upper_name == "ID") score += 100;
                if (upper_name == "SID" || upper_name == "TID" || upper_name == "CID") score += 95;
                if (upper_name.size() > 3 &&
                    upper_name.substr(upper_name.size() - 3) == "_ID") score += 95;
                if (upper_name.find("CODE") != std::string::npos) score += 60;
            }

            if (upper_name == "NAME") score += 90;
            if (upper_name == "LNAME") score += 95;
            if (upper_name == "FNAME") score += 85;
            if (upper_name == "TITLE") score += 80;
            if (upper_name == "GRADE") score += 75;
            if (upper_name == "SEC") score += 70;
            if (upper_name == "DESC" || upper_name == "DESCRIPTION") score += 70;

            if (tm.is_text) score += 20;
            else if (tm.is_numeric) score += 10;
            else if (tm.is_date) score += 8;
            else if (tm.is_logical) score -= 10;

            return score;
        }

        struct FieldCandidate
        {
            int index = -1;
            std::string name;
            TypeMeta type;
            int identity_score = 0;
            int display_score = 0;
        };

        static xbase::DbArea* resolve_area_by_alias(xbase::DbArea& current_area,
                                                    const std::string& alias)
        {
            const std::string want = upper_copy(alias);
            if (want.empty())
                return &current_area;

            try
            {
                if (upper_copy(area_alias_like(current_area)) == want)
                    return &current_area;
            }
            catch (...) {}

            const std::size_t n = workareas::count();
            for (std::size_t i = 0; i < n; ++i)
            {
                xbase::DbArea* a = workareas::at(i);
                if (!a) continue;

                try
                {
                    if (!a->isOpen()) continue;
                }
                catch (...)
                {
                    continue;
                }

                if (upper_copy(area_alias_like(*a)) == want)
                    return a;
            }

            return nullptr;
        }

        static xbase::DbArea* resolve_root_area(xbase::DbArea& current_area,
                                                const BrowserRequest& req,
                                                std::vector<std::string>& warnings,
                                                std::string& status)
        {
            if (req.root_alias.empty())
                return &current_area;

            xbase::DbArea* root = resolve_area_by_alias(current_area, req.root_alias);
            if (root)
                return root;

            warnings.push_back("Root alias not found: " + req.root_alias);
            status = "ERROR";
            return nullptr;
        }

        static bool apply_browser_movement(xbase::DbArea& area,
                                           const BrowserRequest& req,
                                           std::vector<std::string>& warnings,
                                           std::string& status)
        {
            const int moves = (req.move_next ? 1 : 0)
                            + (req.move_prev ? 1 : 0)
                            + (req.move_top ? 1 : 0)
                            + (req.move_bottom ? 1 : 0);

            if (req.refresh_only)
                return true;

            if (moves > 1)
                warnings.push_back("Multiple movement flags supplied; using first recognized movement.");

            if (req.move_next)      { area.skip(1);  return true; }
            if (req.move_prev)      { area.skip(-1); return true; }
            if (req.move_top)       { area.top();    return true; }
            if (req.move_bottom)    { area.bottom(); return true; }

            (void)status;
            return true;
        }

        static bool build_root_snapshot(xbase::DbArea& area,
                                        RecordSnapshot& out,
                                        std::vector<std::string>& warnings,
                                        std::string& status)
        {
            out = RecordSnapshot{};

            out.alias = area_alias_like(area);
            out.recno = area.recno();
            out.bof = area.bof();
            out.eof = area.eof();
            out.deleted = area.isDeleted();

            // If your DbArea has a path getter, wire it here.
            // out.dbf_path = area.path();

            const auto& defs = area.fields();
            if (defs.empty())
            {
                warnings.push_back("No field definitions available for root snapshot.");
                status = "ERROR";
                return false;
            }

            out.fields.clear();
            out.values.clear();

            for (int i = 0; i < static_cast<int>(defs.size()); ++i)
            {
                const auto& fd = defs[static_cast<std::size_t>(i)];

                FieldMeta fm{};
                fm.name = fd.name;
                fm.type = fallback_type_meta_from_code(fd.type);
                fm.length = fd.length;
                fm.decimals = fd.decimals;
                fm.source = out.alias;

                out.fields.push_back(fm);

                // Assumes canonical xbase API: get(1-based index)
                out.values.push_back(area.get(i + 1));
            }

            if (out.fields.size() != out.values.size())
            {
                warnings.push_back("Root snapshot field/value mismatch.");
                status = "ERROR";
                return false;
            }

            return true;
        }

        static bool build_order_snapshot(xbase::DbArea& area,
                                         OrderSnapshot& out,
                                         std::vector<std::string>& warnings,
                                         std::string& status)
        {
            out = OrderSnapshot{};

            // TODO:
            // Replace with your real order/tag/container APIs.
            (void)area;
            (void)warnings;
            (void)status;

            out.has_order = false;
            out.physical_fallback = true;
            return true;
        }

        static void longest_path_dfs(const RelationNode& node,
                                     std::vector<std::string>& current,
                                     std::vector<std::string>& best)
        {
            current.push_back(node.child_alias);

            if (current.size() > best.size())
                best = current;

            for (const auto& child : node.children)
                longest_path_dfs(child, current, best);

            current.pop_back();
        }

        static std::vector<std::string> infer_default_path(const RelationTreeSnapshot& tree)
        {
            std::vector<std::string> best;
            std::vector<std::string> current;

            for (const auto& link : tree.links)
                longest_path_dfs(link, current, best);

            return best;
        }

        static bool validate_path_recursive(const std::vector<RelationNode>& nodes,
                                            const std::vector<std::string>& path,
                                            size_t index)
        {
            if (index >= path.size())
                return true;

            for (const auto& n : nodes)
            {
                if (upper_copy(n.child_alias) == upper_copy(path[index]))
                    return validate_path_recursive(n.children, path, index + 1);
            }

            return false;
        }

        static bool resolve_browser_path(const BrowserRequest& req,
                                         const RelationTreeSnapshot& tree,
                                         std::vector<std::string>& resolved_path,
                                         std::vector<std::string>& warnings,
                                         std::string& status)
        {
            resolved_path.clear();

            if (!req.path_aliases.empty())
            {
                if (!validate_path_recursive(tree.links, req.path_aliases, 0))
                {
                    warnings.push_back("Requested PATH does not match active relation tree.");
                    status = "ERROR";
                    return false;
                }

                resolved_path = req.path_aliases;
                return true;
            }

            resolved_path = infer_default_path(tree);
            return true;
        }

        static bool browser_has_column_expr(const std::vector<TupleColumnMeta>& cols,
                                            const std::string& expr)
        {
            for (const auto& c : cols)
            {
                if (upper_copy(c.expr) == upper_copy(expr))
                    return true;
            }
            return false;
        }

        static bool choose_default_fields_for_alias(xbase::DbArea& area,
                                                    int max_fields,
                                                    std::vector<int>& out_field_indexes,
                                                    std::vector<std::string>& warnings,
                                                    std::string& status)
        {
            out_field_indexes.clear();

            const auto& defs = area.fields();
            if (defs.empty())
            {
                warnings.push_back("No field definitions available for default column inference.");
                status = "WARN";
                return false;
            }

            std::vector<FieldCandidate> candidates;
            candidates.reserve(defs.size());

            for (int i = 0; i < static_cast<int>(defs.size()); ++i)
            {
                const auto& fd = defs[static_cast<std::size_t>(i)];

                FieldCandidate fc{};
                fc.index = i;
                fc.name = fd.name;
                fc.type = fallback_type_meta_from_code(fd.type);

                const std::string u = upper_copy(fd.name);
                fc.identity_score = score_default_field_name(u, fc.type, true);
                fc.display_score  = score_default_field_name(u, fc.type, false);

                candidates.push_back(fc);
            }

            std::set<int> chosen;

            auto pick_best = [&](bool identity_pass) -> bool
            {
                int best_idx = -1;
                int best_score = -1000000;

                for (const auto& c : candidates)
                {
                    if (chosen.count(c.index))
                        continue;

                    const int score = identity_pass ? c.identity_score : c.display_score;
                    if (score > best_score)
                    {
                        best_score = score;
                        best_idx = c.index;
                    }
                }

                if (best_idx >= 0 && best_score > -100)
                {
                    chosen.insert(best_idx);
                    out_field_indexes.push_back(best_idx);
                    return true;
                }

                return false;
            };

            if (static_cast<int>(out_field_indexes.size()) < max_fields)
                pick_best(true);

            while (static_cast<int>(out_field_indexes.size()) < max_fields)
            {
                if (!pick_best(false))
                    break;
            }

            if (static_cast<int>(out_field_indexes.size()) < max_fields)
            {
                for (const auto& c : candidates)
                {
                    if (chosen.count(c.index))
                        continue;
                    if (c.type.is_memo)
                        continue;

                    chosen.insert(c.index);
                    out_field_indexes.push_back(c.index);

                    if (static_cast<int>(out_field_indexes.size()) >= max_fields)
                        break;
                }
            }

            return !out_field_indexes.empty();
        }

        static bool append_default_columns_for_alias(xbase::DbArea& area,
                                                     const std::string& alias,
                                                     int max_fields,
                                                     std::vector<TupleColumnMeta>& out_columns,
                                                     std::vector<std::string>& warnings,
                                                     std::string& status)
        {
            std::vector<int> picks;
            if (!choose_default_fields_for_alias(area, max_fields, picks, warnings, status))
                return false;

            const auto& defs = area.fields();
            if (defs.empty())
                return false;

            for (int idx : picks)
            {
                if (idx < 0 || idx >= static_cast<int>(defs.size()))
                    continue;

                const auto& fd = defs[static_cast<std::size_t>(idx)];

                TupleColumnMeta col{};
                col.source_alias = alias;
                col.expr = alias + "." + fd.name;
                col.label = fd.name;
                col.type = fallback_type_meta_from_code(fd.type);
                col.width = browser_default_width(col.type, fd.length, fd.decimals);
                col.decimals = fd.decimals;

                if (!browser_has_column_expr(out_columns, col.expr))
                    out_columns.push_back(col);
            }

            return true;
        }

        static bool infer_default_columns(xbase::DbArea& root_area,
                                          const std::vector<std::string>& path_aliases,
                                          std::vector<TupleColumnMeta>& out_columns,
                                          std::vector<std::string>& warnings,
                                          std::string& status)
        {
            out_columns.clear();

            append_default_columns_for_alias(root_area,
                                             area_alias_like(root_area),
                                             3,
                                             out_columns,
                                             warnings,
                                             status);

            for (const auto& alias : path_aliases)
            {
                xbase::DbArea* child = resolve_area_by_alias(root_area, alias);
                if (!child)
                {
                    warnings.push_back("Could not resolve area for alias " + alias + " during default column inference.");
                    status = "WARN";
                    continue;
                }

                append_default_columns_for_alias(*child,
                                                 alias,
                                                 2,
                                                 out_columns,
                                                 warnings,
                                                 status);
            }

            return !out_columns.empty();
        }

        static bool resolve_tuple_columns(xbase::DbArea& root_area,
                                          const std::vector<std::string>& path_aliases,
                                          const BrowserRequest& req,
                                          std::vector<TupleColumnMeta>& out_columns,
                                          std::vector<std::string>& warnings,
                                          std::string& status)
        {
            if (!req.column_exprs.empty())
                warnings.push_back("Explicit COLUMNS not yet wired; using inferred default columns.");

            return infer_default_columns(root_area, path_aliases, out_columns, warnings, status);
        }

        static bool build_descendant_summary(xbase::DbArea& root_area,
                                             const std::vector<std::string>& path_aliases,
                                             const std::vector<TupleRow>& rows,
                                             const std::vector<DescendantCount>& counts,
                                             DescendantSummary& out,
                                             std::vector<std::string>& warnings,
                                             std::string& status)
        {
            out = DescendantSummary{};

            (void)root_area;
            (void)warnings;
            (void)status;

            if (!counts.empty())
            {
                out.counts = counts;
                return true;
            }

            for (const auto& alias : path_aliases)
            {
                DescendantCount dc{};
                dc.alias = alias;
                dc.count = rows.empty() ? 0 : static_cast<int>(rows.size());
                out.counts.push_back(dc);
            }

            return true;
        }
    } // namespace

    bool build_browser_snapshot(xbase::DbArea& current_area,
                                const BrowserRequest& req,
                                BrowserSnapshot& out)
    {
        out = BrowserSnapshot{};
        out.limit = (req.limit > 0) ? req.limit : 10;
        out.status = "OK";

        xbase::DbArea* root_area = resolve_root_area(current_area, req, out.warnings, out.status);
        if (!root_area)
            return false;

        if (!apply_browser_movement(*root_area, req, out.warnings, out.status))
            return false;

        if (!build_root_snapshot(*root_area, out.root, out.warnings, out.status))
            return false;

        build_order_snapshot(*root_area, out.order, out.warnings, out.status);

        if (!relation_build_tree(out.root.alias, out.relation_tree, out.warnings, out.status))
            return false;

        if (!resolve_browser_path(req, out.relation_tree, out.path_aliases, out.warnings, out.status))
            return false;

        if (!resolve_tuple_columns(*root_area, out.path_aliases, req, out.columns, out.warnings, out.status))
        {
            out.warnings.push_back("Column resolution failed.");
            out.status = "ERROR";
            return false;
        }

        EnumRequest enum_req{};
        enum_req.root_alias = area_alias_like(*root_area);
        enum_req.path_aliases = out.path_aliases;
        enum_req.columns = out.columns;
        enum_req.limit = out.limit;

        EnumResult enum_res{};
        if (!relation_enumerate(*root_area, enum_req, enum_res))
        {
            out.warnings.insert(out.warnings.end(), enum_res.warnings.begin(), enum_res.warnings.end());
            out.status = enum_res.status.empty() ? "ERROR" : enum_res.status;
            return false;
        }

        out.rows = std::move(enum_res.rows);
        out.warnings.insert(out.warnings.end(), enum_res.warnings.begin(), enum_res.warnings.end());

        if (!enum_res.status.empty() && out.status == "OK")
            out.status = enum_res.status;

        build_descendant_summary(*root_area,
                                 out.path_aliases,
                                 out.rows,
                                 enum_res.counts,
                                 out.descendant_summary,
                                 out.warnings,
                                 out.status);

        out.rows_shown = static_cast<int>(out.rows.size());
        if (out.rows_shown > out.limit)
            out.rows_shown = out.limit;

        if (out.status.empty())
            out.status = out.warnings.empty() ? "OK" : "WARN";

        return true;
    }

    bool build_walk_snapshot(xbase::DbArea& current_area,
                             const BrowserRequest& req,
                             WalkSnapshot& out)
    {
        out = WalkSnapshot{};

        int count = req.walk_count;
        if (count <= 0)
        {
            out.warnings.push_back("WALK count <= 0; nothing to build.");
            out.status = "WARN";
            return true;
        }

        BrowserRequest frame_req = req;
        frame_req.walk_mode = false;
        frame_req.limit = (req.walk_limit > 0) ? req.walk_limit : req.limit;

        for (int i = 0; i < count; ++i)
        {
            BrowserSnapshot snap{};
            if (!build_browser_snapshot(current_area, frame_req, snap))
            {
                out.warnings.push_back("Failed to build WALK frame " + std::to_string(i + 1) + ".");
                out.status = "WARN";
                break;
            }

            out.frames.push_back(std::move(snap));

            frame_req.refresh_only = false;
            frame_req.move_next = true;
            frame_req.move_prev = false;
            frame_req.move_top = false;
            frame_req.move_bottom = false;
        }

        if (out.status.empty())
            out.status = out.warnings.empty() ? "OK" : "WARN";

        return true;
    }
}