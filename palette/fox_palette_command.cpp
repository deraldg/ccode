// File: fox_palette_command.cpp
// Loadable entry for DotTalk++ command registry: cmd_FOX_PALETTE
// No DotTalk headers required; we forward-declare to stay decoupled.
#include <sstream>
#include <tvision/tv.h>
#include "cmd_fox_palette.h"   // TTestApp

namespace xbase { class DbArea; } // forward-declare only, to match DotTalk++ signature

#if defined(_WIN32)
#  define DOTTALK_EXPORT extern "C" __declspec(dllexport)
#else
#  define DOTTALK_EXPORT extern "C"
#endif

// Signature mirrors your other commands (e.g., cmd_FOXPRO).
DOTTALK_EXPORT void cmd_FOX_PALETTE(xbase::DbArea& /*area*/, std::istringstream& /*args*/) {
#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE) || 1
    TTestApp app;
    app.run();
#else
    // If you guard TV at build-time, keep the same messaging contract.
    // std::cout << "TVISION is not available in this build.\n";
#endif
}
