// include/browser/browser_snapshot.hpp
#pragma once

#include <string>
#include <vector>

namespace browser
{
    struct TypeMeta
    {
        std::string code;      // "C", "N", "D", etc.
        std::string name;      // "Character", "Numeric", etc.

        bool is_numeric = false;
        bool is_text = false;
        bool is_date = false;
        bool is_logical = false;
        bool is_memo = false;
    };

    struct FieldMeta
    {
        std::string name;
        TypeMeta type;
        int length = 0;
        int decimals = 0;
        std::string source;
    };

    struct RecordSnapshot
    {
        std::string alias;
        std::string dbf_path;
        int recno = 0;
        bool bof = false;
        bool eof = false;
        bool deleted = false;

        std::vector<FieldMeta> fields;
        std::vector<std::string> values;
    };

    struct RelationNode
    {
        std::string parent_alias;
        std::string child_alias;
        std::string on_expr;
        std::vector<RelationNode> children;
    };

    struct RelationTreeSnapshot
    {
        std::string root_alias;
        std::vector<RelationNode> links;
    };

    struct DescendantCount
    {
        std::string alias;
        int count = 0;
    };

    struct DescendantSummary
    {
        std::vector<DescendantCount> counts;
    };

    struct TupleColumnMeta
    {
        std::string source_alias;
        std::string expr;
        std::string label;
        TypeMeta type;
        int width = 0;
        int decimals = 0;
    };

    struct TupleRow
    {
        std::vector<std::string> cells;
    };

    struct OrderSnapshot
    {
        bool has_order = false;
        std::string container_path;
        std::string tag_name;
        std::string direction;
        bool physical_fallback = false;
    };

    struct BrowserRequest
    {
        std::string root_alias;
        std::vector<std::string> path_aliases;
        std::vector<std::string> column_exprs;
        int limit = 10;

        bool move_next = false;
        bool move_prev = false;
        bool move_top = false;
        bool move_bottom = false;
        bool refresh_only = false;

        bool walk_mode = false;
        int walk_count = 0;
        int walk_limit = 10;
    };

    struct BrowserSnapshot
    {
        std::string command_name = "ERSATZ";

        RecordSnapshot root;
        RelationTreeSnapshot relation_tree;
        DescendantSummary descendant_summary;
        OrderSnapshot order;

        std::vector<std::string> path_aliases;
        std::vector<TupleColumnMeta> columns;
        std::vector<TupleRow> rows;

        int limit = 10;
        int rows_shown = 0;

        std::vector<std::string> warnings;
        std::string status = "OK";
    };

    struct WalkSnapshot
    {
        std::vector<BrowserSnapshot> frames;
        std::vector<std::string> warnings;
        std::string status = "OK";
    };
}
