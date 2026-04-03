#pragma once

#include <string>

// ---- TVision uses ----
#define Uses_TWindow
#define Uses_TRect

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

class FoxtalkLogView;

class FoxtalkOutputWindow : public TWindow {
public:
    FoxtalkOutputWindow(const TRect& bounds, const char* title, ushort number = 0);

    FoxtalkLogView* logView() const;

private:
    FoxtalkLogView* logView_{nullptr};
};

} // namespace foxtalk