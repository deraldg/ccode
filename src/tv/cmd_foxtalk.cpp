#include <iostream>
#include <sstream>

#include "tv/foxtalk_app.hpp"
#include "xbase.hpp"

void cmd_FOXTALK(xbase::DbArea&, std::istringstream&)
{
#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE)
    foxtalk::TFoxtalkApp app;
    std::cout << "Launching Foxtalk UI...\n";
    app.run();
#else
    std::cout << "TVISION is not available in this build.\n";
#endif
}