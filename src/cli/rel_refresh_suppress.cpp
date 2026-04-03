#include "cli/rel_refresh_suppress.hpp"

#include <string_view>

// -----------------------------------------------------------------------------
// Relations auto-refresh suppression
// -----------------------------------------------------------------------------

static thread_local int g_rel_refresh_suppress = 0;

extern "C" void shell_rel_refresh_push() noexcept
{
    ++g_rel_refresh_suppress;
}

extern "C" void shell_rel_refresh_pop() noexcept
{
    if (g_rel_refresh_suppress > 0) {
        --g_rel_refresh_suppress;
    }
}

extern "C" bool shell_rel_refresh_is_suspended() noexcept
{
    return g_rel_refresh_suppress > 0;
}

bool shell_is_rel_refresh_suppression_command(std::string_view U) noexcept
{
    // Keep this list behavior-identical to the original shell.cpp block.
    // Caller should pass an already-uppercased command token.
    return (U == "COUNT"   || U == "LIST"     || U == "DISPLAY"  || U == "SMARTLIST" || U == "AGGS"
         || U == "SUM"     || U == "AVG"      || U == "AVERAGE"  || U == "MIN"       || U == "MAX"
         || U == "WHERE"   || U == "WSREPORT" || U == "VALIDATE" || U == "NORMALIZE"
         || U == "REBUILD" || U == "INDEX"    || U == "REINDEX"  || U == "DELETE");
}
