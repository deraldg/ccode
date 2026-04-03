#pragma once

// ---- TVision uses ----
#define Uses_TRect
#define Uses_TStatusLine

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

TStatusLine* buildStatusLine(TRect bounds);

} // namespace foxtalk