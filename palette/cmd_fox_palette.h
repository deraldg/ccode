// ccode/palette/cmd_fox_palette.h  (FULL REPLACEMENT)
#ifndef _CMD_FOX_PALETTE_H
#define _CMD_FOX_PALETTE_H

// Tell tv.h which symbols we need.
#define Uses_TApplication
#define Uses_TMenuBar
#define Uses_TRect
#define Uses_TEvent
#define Uses_TPalette

#if __has_include(<tvision/tv.h>)
  #include <tvision/tv.h>
#elif __has_include(<tv.h>)
  #include <tv.h>
#else
  #error "Turbo Vision header not found: expected <tvision/tv.h> or <tv.h>"
#endif

class TTestApp : public TApplication
{
public:
    TTestApp();
    static TMenuBar* initMenuBar(TRect r);
    void handleEvent(TEvent& event) override;
    TPalette& getPalette() const override;

private:
    void aboutDlg();
    void paletteView();
};

#endif // _CMD_FOX_PALETTE_H
