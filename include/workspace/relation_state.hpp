#pragma once

#include <string>

namespace dottalk::workspace
{

struct RelationState
{
    int parent_slot = -1;
    int child_slot  = -1;

    std::string parent_alias;
    std::string child_alias;

    std::string expression;

    bool is_valid() const noexcept
    {
        const bool by_slot =
            (parent_slot >= 0) &&
            (child_slot >= 0) &&
            !expression.empty();

        const bool by_alias =
            !parent_alias.empty() &&
            !child_alias.empty() &&
            !expression.empty();

        return by_slot || by_alias;
    }
};

} // namespace dottalk::workspace
