// ============================================================================
// File: src/cli/foxtalk_palettes.cpp
// Purpose: Stub TU. Palettes are embedded in cmd_foxtalk.cpp.
// Note   : We only declare applyRetroPalette here; the definition lives in
//          cmd_foxtalk.cpp to avoid multiple definition/link errors.
// ============================================================================

#include <string>

namespace foxtalk {
    // Defined in cmd_foxtalk.cpp
    void applyRetroPalette(const std::string& name);
}
