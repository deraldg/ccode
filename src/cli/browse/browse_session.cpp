#include "browse_session.hpp"
#include "browse_args.hpp"
#include "browse_format.hpp"
#include "browse_filters.hpp"
#include "browse_order.hpp"
#include "browse_edit.hpp"
#include "browse_io.hpp"

#include "xbase.hpp"
#include <iostream>
#include <sstream>

namespace dottalk::browse {

BrowseSession::BrowseSession(::xbase::DbArea& area,
                             const BrowseArgs& args,
                             const order::OrderView& ord,
                             const filters::ForProgram& prog)
  : area_(area), args_(args), ord_(ord), prog_(prog) {}

void BrowseSession::run(){
    // Skeleton: show current record and a small prompt loop that can quit.
    io::print_status(area_, order::summarize_order(area_), dirty());
    std::string line;
    io::print_help_inline();
    for (;;){
        std::cout << "BROWSE> ";
        if (!std::getline(std::cin, line)) break;
        std::istringstream iss(line);
        std::string cmd; iss >> cmd;
        if (cmd.empty()) continue;
        for (auto& c : cmd) c = (char)std::toupper((unsigned char)c);

        if (cmd == "Q" || cmd == "QUIT"){
            if (dirty()){
                std::cout << "You have staged changes. SAVE or CANCEL first.\n";
                continue;
            }
            break;
        }
        else if (cmd == "H" || cmd == "?"){
            io::print_help_inline();
        }
        else if (cmd == "STATUS"){
            io::print_status(area_, order::summarize_order(area_), dirty());
        }
        else {
            std::cout << "Unknown command (skeleton). Type ? for help.\n";
        }
    }
}

} // namespace dottalk::browse
