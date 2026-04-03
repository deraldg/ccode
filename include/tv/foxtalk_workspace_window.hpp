#pragma once

// ---- TVision uses ----
#define Uses_TWindow
#define Uses_TRect

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

class FoxtalkWorkspaceView;
struct WorkspaceSnapshot;

class FoxtalkWorkspaceWindow : public TWindow {
public:
    FoxtalkWorkspaceWindow(const TRect& bounds, const char* title, ushort number = 0);

    FoxtalkWorkspaceView* workspaceView() const;
    void setSnapshot(const WorkspaceSnapshot& snapshot);
    void clearSnapshot();

private:
    FoxtalkWorkspaceView* workspaceView_{nullptr};
};

} // namespace foxtalk
