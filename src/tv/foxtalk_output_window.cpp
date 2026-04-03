#include "tv/foxtalk_output_window.hpp"
#include "tv/foxtalk_log_view.hpp"
#include "tv/foxtalk_util.hpp"

// ---- TVision uses ----
#define Uses_TScrollBar

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

FoxtalkOutputWindow::FoxtalkOutputWindow(const TRect& bounds, const char* title, ushort number)
    : TWindow(bounds, title, number)
    , TWindowInit(&TWindow::initFrame)
{
    flags |= (wfMove | wfGrow | wfZoom);
    growMode = gfGrowAll;

    const int ww = static_cast<int>(bounds.b.x - bounds.a.x);
    const int wh = static_cast<int>(bounds.b.y - bounds.a.y);

    auto* h = new TScrollBar(TRect(S(1), S(wh - 2), S(ww - 2), S(wh - 1)));
    auto* v = new TScrollBar(TRect(S(ww - 2), S(1), S(ww - 1), S(wh - 2)));

    insert(h);
    insert(v);

    logView_ = new FoxtalkLogView(
        TRect(S(1), S(1), S(ww - 2), S(wh - 2)),
        h,
        v
    );
    logView_->setMaxLines(100000);
    insert(logView_);
}

FoxtalkLogView* FoxtalkOutputWindow::logView() const
{
    return logView_;
}

} // namespace foxtalk