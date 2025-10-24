#include "test.h"
#include "xbase.hpp"
#include <sstream>
#include <iostream>

void cmd_TTESTAPP(xbase::DbArea& /*db*/, std::istringstream& /*args*/)
{
#ifdef DOTTALK_TV_AVAILABLE
    TTestApp app;
    app.run();
#else
    std::cout << "TTESTAPP: Turbo Vision not available in this build.\n";
#endif
}
