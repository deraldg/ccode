#include <iostream>
#include "cli/output_router.hpp"
#include "post_poll.hpp"

void post_poll()
{
    auto& out = cli::OutputRouter::instance().out();
    out << "[POLL POST]\n";
}
