// include/browser/browser_relation_adapter.hpp
#pragma once

#include <string>
#include <vector>

#include "browser/browser_snapshot.hpp"
#include "xbase.hpp"

namespace browser
{
    struct EnumRequest
    {
        std::string root_alias;
        std::vector<std::string> path_aliases;
        std::vector<TupleColumnMeta> columns;
        int limit = 10;
    };

    struct EnumResult
    {
        std::vector<TupleRow> rows;
        std::vector<DescendantCount> counts;
        std::vector<std::string> warnings;
        std::string status = "OK";
    };

    bool relation_build_tree(const std::string& root_alias,
                             RelationTreeSnapshot& out_tree,
                             std::vector<std::string>& warnings,
                             std::string& status);

    bool relation_enumerate(xbase::DbArea& root_area,
                            const EnumRequest& req,
                            EnumResult& out);
}
