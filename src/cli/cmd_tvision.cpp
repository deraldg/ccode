// src/cli/cmd_tvision.cpp — ALWAYS compiled into dottalkpp.
// Launches Turbo Vision UI when available, otherwise prints a helpful message.

#include <iostream>
#include <sstream>
#include "xbase.hpp"
#include "hello.hpp"

// Only include TV headers and run UI when CMake found the package.
#ifdef DOTTALK_TV_AVAILABLE
  #define Uses_TApplication
  #define Uses_TDialog
  #define Uses_TRect
  #define Uses_TEvent
  #define Uses_MsgBox
  #include <tvision/tv.h>
#endif

void cmd_TVISION(xbase::DbArea& /*area*/, std::istringstream& /*args*/)
{
#ifdef DOTTALK_TV_AVAILABLE
    std::cout << "Launching Turbo Vision UI...\n";

    // Instantiate and run THelloApp
    THelloApp app;
    app.run();

#else
    std::cout << "TVISION is not available in this build (library not found at configure time).\n";
    std::cout << "Tip: Install tvision (vcpkg or local), enable manifest mode/toolchain, re-configure, and rebuild.\n";
#endif
}