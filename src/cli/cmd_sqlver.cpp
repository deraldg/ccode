// src/cli/cmd_sqlver.cpp
#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"

#if DOTTALK_SQLITE_AVAILABLE
  #include <sqlite3.h>
#endif

// Shell entry point: SQLVER
// Usage: . SQLVER
void cmd_SQLVER(xbase::DbArea& /*area*/, std::istringstream& /*is*/)
{
#if DOTTALK_SQLITE_AVAILABLE
    const char* ver = sqlite3_libversion();
    std::cout << "SQLite available: 1, version: " << (ver ? ver : "unknown") << "\n";
#else
    std::cout << "SQLite available: 0\n";
#endif
}
