#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE)
#ifndef _TEST_H
#define _TEST_H

#define Uses_TApplication
#define Uses_TMenuBar
#define Uses_TStatusLine      // <-- add this line
#define Uses_TRect
#define Uses_TEvent
#define Uses_TPalette
#define Uses_TSubMenu         // <-- and these two for the builders below
#define Uses_TMenuItem
#include <tvision/tv.h>

class TTestApp : public TApplication
{
public:
    TTestApp();
    static TMenuBar*    initMenuBar(TRect r);
    static TStatusLine* initStatusLine(TRect r);   // <-- add this
    virtual void handleEvent(TEvent& event);
    virtual TPalette& getPalette() const;
private:
    void aboutDlg();
    void paletteView();
};

#endif // _TEST_H
#else
// TVision not enabled: header declarations not required in non-TV build.

#endif  // TVISION guard
