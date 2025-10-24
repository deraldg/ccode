// include/palette_test.h
#ifndef PALETTE_TEST_H
#define PALETTE_TEST_H

#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TPalette
#include <tvision/tv.h>

class TTestApp : public TApplication
{
public:
    TTestApp();
    static TMenuBar *initMenuBar(TRect r);
    static TStatusLine *initStatusLine(TRect r);
    virtual void handleEvent(TEvent& event);
    virtual TPalette& getPalette() const;
private:
    void aboutDlg();
    void paletteView();
};

#endif // PALETTE_TEST_H