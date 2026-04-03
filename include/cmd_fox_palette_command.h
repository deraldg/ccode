// ccode/include/cmd_fox_palette_command.h
#pragma once
#include <sstream>
namespace xbase { class DbArea; }      // matches other commands

// C++ linkage on purpose: this is what shell.cpp expects to call.
void cmd_FOX_PALETTE(xbase::DbArea&, std::istringstream&);



