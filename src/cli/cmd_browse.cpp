// src/cli/cmd_browse.cpp
//
// DotTalk++ — BROWSE command handler (V1/V2 switch)
// Default: legacy Browsetui (V1) runs exactly as it does today.
// Opt-in: set env BROWSE_TUI_V2=1 to run V2 pilot (interfaces + stubs).

#include <sstream>
#include <iostream>
#include <memory>

// Bring in DbArea
#include "xbase.hpp"

// V2 interfaces/stubs
#include "browse/browse_flags.hpp"
#include "browse/controller.hpp"
#include "data/irecord_gateway.hpp"
#include "ui/ibrowse_renderer.hpp"
#include "input/iinput_source.hpp"

// -----------------------------------------------------------------------------
// Legacy path (V1)
// If your original cmd_browse.cpp had inline legacy code, paste it into
// run_browsetui_v1(). If you already have a function elsewhere, declare
// it as extern below and call it.
// -----------------------------------------------------------------------------

// OPTION B (call out to an existing function somewhere else):
// extern void legacy_browse(xbase::DbArea&, std::istringstream&);

static void run_browsetui_v1(xbase::DbArea& area, std::istringstream& args)
{
    // If you have an existing function, call it here instead:
    // legacy_browse(area, args);

    // TEMP placeholder so this compiles/links even if you haven't wired V1 yet.
    std::cerr << "[browse] V1 path placeholder — wire your current Browsetui call here.\n";
    (void)area; (void)args;
}

// -----------------------------------------------------------------------------
// Public command entry (registry dispatches here)
// Signature matches other command handlers (void, not int).
// -----------------------------------------------------------------------------
void cmd_BROWSE(xbase::DbArea& area, std::istringstream& args)
{
    // === Legacy by default ===
    if (!browse::use_v2()) {
        run_browsetui_v1(area, args);
        return;
    }

    // === V2 pilot (safe/read-only) ===
    std::unique_ptr<browse::IRecordGateway> gw(browse::create_dbf_gateway_stub());
    std::unique_ptr<browse::IBrowseRenderer> r (browse::create_renderer_v2());
    std::unique_ptr<browse::IInputSource>   in(browse::create_legacy_input_adapter());

    browse::Controller ctl(*gw, *r, *in);

    // TODO: replace with actual terminal size from your TUI backend
    const int termW = 120;
    const int termH = 40;

    ctl.init_vm(termW, termH, browse::windowed_default());

    while (ctl.tick()) {
        // loop until Quit → returns to shell
    }
}
