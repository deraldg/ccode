#include "browse_io.hpp"
#include "browse_args.hpp"
#include "xbase.hpp"
#include <iostream>

namespace dottalk::browse::io {

bool prompt_more(bool quiet){
    if (quiet) return true;

    // Pager prompt: this is intentionally not a full command shell.
    // If the user types something other than Enter/Q, we continue paging
    // but print a one-time hint explaining how to run follow-on commands.
    static bool showed_hint = false;

    std::cout << "-- More -- (Enter to continue, Q to quit) ";
    std::cout.flush();

    std::string line;
    if (!std::getline(std::cin, line)) return false;

    if (!line.empty() && (line[0] == 'q' || line[0] == 'Q')) return false;

    if (!line.empty() && !showed_hint){
        showed_hint = true;
        std::cout
            << "BROWSE: pager mode. Press Q to exit, then run commands at the dot prompt (e.g., BROWSE FOR <expr>).\n";
    }
    return true;
}

void print_banner(::xbase::DbArea& /*area*/, const BrowseArgs& args, const std::string& order_summary){
    if (args.quiet) return;
    std::cout << "Entered BROWSE mode (read-only).\n";
    std::cout << "ORDER: " << order_summary << "\n";
    std::cout << "Format: " << (args.want_raw ? "RAW" : "PRETTY")
              << " | Start: " << (args.start == BrowseArgs::StartPos::Top ? "TOP" : "BOTTOM")
              << " | Page: " << args.page_size;
    if (!args.for_expr.empty()) std::cout << " | FOR: " << args.for_expr;
    if (!args.start_key_literal.empty()) std::cout << " | START KEY: " << args.start_key_literal;
    std::cout << "\n\n";
}

void print_status(::xbase::DbArea& area, const std::string& order_summary, bool dirty){
    std::cout << "STATUS: recno=" << area.recno()
              << " deleted=" << (area.isDeleted() ? "Y" : "N")
              << " dirty=" << (dirty ? "Y" : "N")
              << " order=" << order_summary
              << "\n";
}

void print_help_inline(){
    std::cout <<
        "Commands:\n"
        "  N / P          - Next / Previous (order-aware, respects FOR)\n"
        "  G <recno>      - Go to record number\n"
        "  E [<field> [WITH <value>]]  - Edit current record (prompt if <field> omitted)\n"
        "  SAVE / CANCEL  - Commit or discard staged edits\n"
        "  DEL / RECALL   - Mark deleted / Undelete current record\n"
        "  STATUS         - Show rec/order/deleted/dirty\n"
        "  H / ?          - Help\n"
        "  Q              - Quit\n";
}

} // namespace dottalk::browse::io
