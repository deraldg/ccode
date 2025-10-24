// ccode/src/cli/cmd_fox_palette_entry.cpp  (FULL REPLACEMENT)
#include <sstream>
#include "cmd_fox_palette_command.h"
#include "cmd_fox_palette.h"   // brings TApplication via tv.h + Uses_* above

namespace xbase { class DbArea; }

static void run_palette_app()
{
    TTestApp app;
    app.run();
}

void cmd_FOX_PALETTE(xbase::DbArea& /*area*/, std::istringstream& /*args*/)
{
    run_palette_app();
}
