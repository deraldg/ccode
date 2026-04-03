#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>

#include "browse_args.hpp"
#include "browse_order.hpp"
#include "browse_filters.hpp"

namespace xbase { class DbArea; }
namespace dottalk::browse { struct BrowseArgs; }
namespace dottalk::browse::filters { struct ForProgram; }
namespace dottalk::browse::order { struct OrderView; }

namespace dottalk::browse {

class BrowseSession {
public:
    BrowseSession(::xbase::DbArea& area,
                  const BrowseArgs& args,
                  const order::OrderView& ord,
                  const filters::ForProgram& prog);

    void run(); // N/P/G/E/SAVE/CANCEL/DEL/RECALL/STATUS/HELP/Q

private:
    ::xbase::DbArea& area_;
    BrowseArgs args_;
    order::OrderView ord_;
    filters::ForProgram prog_;
    std::map<int,std::string> staged_;
    int cur_idx_ = -1;

    bool dirty() const { return !staged_.empty(); }
};

} // namespace dottalk::browse
