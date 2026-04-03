// include/browser/browser_builders.hpp
#pragma once

#include "xbase.hpp"
#include "browser/browser_snapshot.hpp"

namespace browser
{
    bool build_browser_snapshot(xbase::DbArea& current_area,
                                const BrowserRequest& req,
                                BrowserSnapshot& out);

    bool build_walk_snapshot(xbase::DbArea& current_area,
                             const BrowserRequest& req,
                             WalkSnapshot& out);
}
