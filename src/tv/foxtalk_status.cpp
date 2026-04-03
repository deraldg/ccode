// ---- TVision uses ----
#define Uses_TRect
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TKeys

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

#include "tv/foxtalk_status.hpp"
#include "tv/foxtalk_ids.hpp"
#include "tv/foxtalk_util.hpp"

namespace foxtalk {

TStatusLine* buildStatusLine(TRect bounds)
{
    bounds.a.y = S(bounds.b.y - 1);

    return new TStatusLine(
        bounds,
        *new TStatusDef(0, 0xFFFF)
            + *new TStatusItem("~F10~ Menu",            kbF10,    cmMenu)
            + *new TStatusItem("~Ctrl-Q~ Command",      kbCtrlQ,  cmFocusCmd)
            + *new TStatusItem("~F2~ Output",           kbF2,     cmShowOutput)
            + *new TStatusItem("~F3~ Record",           kbF3,     0)
            + *new TStatusItem("~F4~ Workspace",        kbF4,     cmWorkspace)
            + *new TStatusItem("~Alt-Z~ Zoom Cmd",      kbAltZ,   cmZoomCmd)
            + *new TStatusItem("~Ctrl-F5~ Restore Cmd", kbCtrlF5, cmResetCmdWin)
            + *new TStatusItem("~Alt-O~ Zoom Out",      kbAltO,   cmZoomOut)
            + *new TStatusItem("~Ctrl-F6~ Restore Out", kbCtrlF6, cmResetOutWin)
            + *new TStatusItem("~PgUp/Dn/Home/End~",    kbPgDn,   0)
            + *new TStatusItem("~Esc/Ctrl-U/K~",        kbEsc,    0)
            + *new TStatusItem("~Alt-X~ Exit",          kbAltX,   cmTalkExit)
    );
}

} // namespace foxtalk