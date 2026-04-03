#pragma once

#include <sstream>

namespace xbase
{
    class DbArea;
}

void cmd_IMPORTSQL(xbase::DbArea& area, std::istringstream& iss);
void cmd_EXPORTSQL(xbase::DbArea& area, std::istringstream& iss);