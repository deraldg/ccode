#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE)
// include/palette_app.h
#ifndef PALETTE_APP_H
#define PALETTE_APP_H

#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TPalette
#include <tvision/tv.h>

class TPaletteApp : public TApplication
{
public:
    TPaletteApp();
    static TMenuBar *initMenuBar(TRect r);
    static TStatusLine *initStatusLine(TRect r);
    virtual void handleEvent(TEvent& event);
    virtual TPalette& getPalette() const;
private:
    void aboutDlg();
    void paletteView();
};

#endif // PALETTE_APP_H
#else
// TVision not enabled: header declarations not required in non-TV build.

#endif  // TVISION guard
