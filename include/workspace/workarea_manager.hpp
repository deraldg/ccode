
#pragma once

#include "xbase.hpp"

namespace dottalk::workspace {

class WorkAreaManager
{
public:
    // Align with engine capacity
    int count() const noexcept { return xbase::MAX_AREA; }

    int current_slot() const noexcept;

    bool select(int slot);

    xbase::DbArea* dbarea(int slot) noexcept;
    const xbase::DbArea* dbarea(int slot) const noexcept;

    xbase::DbArea* current_dbarea() noexcept;
    const xbase::DbArea* current_dbarea() const noexcept;
};

}
