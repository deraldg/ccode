#include "tv/foxtalk_workspace_window.hpp"
#include "tv/foxtalk_util.hpp"
#include "tv/foxtalk_workspace_view.hpp"

namespace foxtalk {

FoxtalkWorkspaceWindow::FoxtalkWorkspaceWindow(const TRect& bounds, const char* title, ushort number)
    : TWindow(bounds, title, number)
    , TWindowInit(&TWindow::initFrame)
{
    flags |= (wfMove | wfGrow | wfZoom);
    growMode = gfGrowHiX | gfGrowHiY;

    const int ww = static_cast<int>(bounds.b.x - bounds.a.x);
    const int wh = static_cast<int>(bounds.b.y - bounds.a.y);

    workspaceView_ = new FoxtalkWorkspaceView(
        TRect(S(1), S(1), S(ww - 1), S(wh - 1))
    );
    insert(workspaceView_);
}

FoxtalkWorkspaceView* FoxtalkWorkspaceWindow::workspaceView() const
{
    return workspaceView_;
}

void FoxtalkWorkspaceWindow::setSnapshot(const WorkspaceSnapshot& snapshot)
{
    if (workspaceView_)
        workspaceView_->setSnapshot(snapshot);
}

void FoxtalkWorkspaceWindow::clearSnapshot()
{
    if (workspaceView_)
        workspaceView_->clearSnapshot();
}

} // namespace foxtalk