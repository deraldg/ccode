#include "shell_buffer_utils.hpp"

#include "scan_state.hpp"
#include "loop_state.hpp"
#include "command_registry.hpp"

// WHILE uses its own private buffer/state in cmd_while.cpp
extern "C" bool while_is_active();

namespace dottalk {

bool handle_buffers_if_active(xbase::XBaseEngine& eng,
                              const std::string& U,
                              const std::string& line_for_scan,
                              const std::string& line_for_loop)
{
    using namespace dli;

    xbase::DbArea& curCap = eng.area(eng.currentArea());

    // SCAN has its own buffer command.
    if (scanblock::state().active && !is_match(U, "ENDSCAN", "END SCAN")) {
        std::istringstream cap(line_for_scan);
        registry().run(curCap, "SCAN_BUFFER", cap);
        return true;
    }

    // WHILE uses its own private state/body buffer in cmd_while.cpp.
    if (while_is_active() &&
        !is_match(U, "ENDWHILE", "END WHILE")) {
        std::istringstream cap(line_for_loop);
        registry().run(curCap, "WHILE_BUFFER", cap);
        return true;
    }

    // LOOP / UNTIL continue to use the shared loopblock buffer.
    if (loopblock::state().active &&
        !is_match(U, "ENDLOOP", "END LOOP") &&
        !is_match(U, "ENDWHILE", "END WHILE") &&
        !is_match(U, "ENDUNTIL", "END UNTIL")) {
        std::istringstream cap(line_for_loop);
        registry().run(curCap, "LOOP_BUFFER", cap);
        return true;
    }

    return false;
}

} // namespace dottalk