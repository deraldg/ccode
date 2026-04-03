// File: src/cli/cmd_lmdb_util.cpp
//
// LMDB_UTIL is deprecated.
//
// The project has moved to the OO model:
//   DbArea -> IndexManager -> CdxBackend -> MDB_env*
//
// Use the per-area LMDB command instead:
//   LMDB INFO
//   LMDB OPEN <container.cdx | envdir.cdx.d | stem>
//   LMDB USE  <TAG>
//   LMDB SEEK <key>
//   LMDB DUMP [<max>]
//   LMDB SCAN <low> <high>
//   LMDB CLOSE
//
// This stub intentionally does NOT open any LMDB environments or transactions,
// to avoid cross-area contamination and reader-slot conflicts.
//
#include <iostream>
#include <sstream>

#include "xbase.hpp"

void cmd_LMDB_UTIL(xbase::DbArea& /*a*/, std::istringstream& /*iss*/)
{
    std::cout
        << "LMDB_UTIL is deprecated and disabled.\n"
        << "Use: LMDB (per-area)\n";
}
