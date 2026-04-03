#pragma once

// ---- TVision uses ----
#define Uses_TRect
#define Uses_TMenuBar

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

TMenuBar* buildMenuBar(TRect bounds);

} // namespace foxtalk