#include <iostream>
#include "cli/output_router.hpp"
#include "pre_poll.hpp"

void pre_poll()
{
    auto& out = cli::OutputRouter::instance().out();
    out << "[POLL PRE]\n";
}
