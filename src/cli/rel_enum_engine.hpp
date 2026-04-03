// src/cli/rel_enum_engine.hpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace xbase { class DbArea; }

namespace rel_enum_engine
{
    struct Request
    {
        std::string root_alias;
        std::vector<std::string> path_aliases;
        std::vector<std::string> tuple_exprs;
        std::size_t limit = 0;   // 0 = unlimited (matches existing REL ENUM behavior)
        bool distinct = false;
    };

    struct Row
    {
        std::vector<std::string> cells;
    };

    struct Count
    {
        std::string alias;
        int count = 0;
    };

    struct Result
    {
        std::vector<Row> rows;
        std::vector<Count> counts;
        std::vector<std::string> warnings;
        std::string status = "OK";
    };

    bool run(xbase::DbArea& area, const Request& req, Result& out);
}