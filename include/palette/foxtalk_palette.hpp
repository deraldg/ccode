#pragma once
// ============================================================================
// Foxtalk Palette Engine (magiblot TVision v4)
// Provides named palettes + helpers and a single entry point:
//
//   foxtalk::applyRetroPalette("<name>");   // "" => FOXPRO default
//
// Names (case-insensitive):
//   FOXPRO (aliases: FOX, 2.6, DOS), DBASE, CLIPPER, AMBER, MATRIX, NEON
//
// Extra helpers (optional):
//   foxtalk::currentPaletteName();      // last applied (process-local)
//   foxtalk::paletteNames();            // list of valid names
//   foxtalk::nextPaletteName("<name>"); // ring-iterate for hotkey cycling
//
// Drop-in usage:
//   1) Add this header + the .cpp below to your tree.
//   2) Remove the old palette arrays / apply function from cmd_foxtalk.cpp.
//   3) #include "palette/foxtalk_palette.hpp" where you need it.
//   4) Keep using applyRetroPalette(...) exactly as before.
// ============================================================================

#include <string>
#include <vector>

namespace foxtalk {

// Apply a named palette (empty => FOXPRO default). Prints a short note to stdout.
// Safe to call any time after TProgram::application is created.
void applyRetroPalette(const std::string& name);

// Convenience helpers (non-persistent, for UI and cycling).
const std::string& currentPaletteName();
const std::vector<std::string>& paletteNames();
std::string nextPaletteName(const std::string& from);

} // namespace foxtalk
