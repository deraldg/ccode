#include "browse_cmd.hpp"
#include "browse_args.hpp"
#include "browse_format.hpp"
#include "browse_filters.hpp"
#include "browse_order.hpp"
#include "browse_session.hpp"
#include "browse_io.hpp"

#include "xbase.hpp"
#include "cli/settings.hpp"


#include <limits>
#include <sstream>
#include <iostream>

namespace dottalk::browse {

void cmd_BROWSE(::xbase::DbArea& area, std::istringstream& in){
    // 1) Parse args
    BrowseArgs args = parse_args(in);

    // 2) Order + FOR program (skeletons now; migrate real logic later)
    auto ord = order::build_order_view(area);
    auto prog = filters::compile_for(args.for_expr);

    // 3) Start position + START KEY
    order::apply_start(area, args.start);
    if (!args.start_key_literal.empty()){
        order::apply_start_key_seek(area, args.start_key_literal);
    }

    // 4) Banner
    io::print_banner(area, args, order::summarize_order(area));

    // 5) Non-interactive listing (skeleton: lists current record once per page)
    if (!args.interactive){
        int shown = 0;
        const long limit = args.list_all ? std::numeric_limits<long>::max() : args.page_size;

        // Minimal walk: from current forward using physical skip (until real order is migrated)
        while (true){
            if (area.readCurrent() && filters::record_visible(area, prog, cli::Settings::instance().deleted_on.load())){
                std::string line = args.want_raw ? format::tuple_raw(area) : format::tuple_pretty(area);
                std::cout << line << "\n";
                ++shown;
                if (shown >= limit && !args.list_all){
                    shown = 0;
                    if (!io::prompt_more(args.quiet)) break;
                }
            }
            if (!area.skip(+1)) break;
        }
        if (!args.quiet) std::cout << "(skeleton listing complete)\n";
        return;
    }

    // 6) Interactive session
    BrowseSession session(area, args, ord, prog);
    session.run();
}

} // namespace dottalk::browse
