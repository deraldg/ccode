#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE)
#pragma once

#define Uses_TKeys
#define Uses_TPoint
#define Uses_TRect
#define Uses_TEvent
#define Uses_TView
#define Uses_TGroup
#define Uses_TWindow
#define Uses_TDeskTop
#define Uses_TApplication
#define Uses_TMenu
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStaticText
#define Uses_MsgBox
#include <tvision/tv.h>

// ... your other common STL/system includes as desired
#else
// TVision not enabled: header declarations not required in non-TV build.

#endif  // TVISION guard
