// ccode/palette/fox_palette.h  (FULL REPLACEMENT)
#ifndef _FOX_PALETTE_H
#define _FOX_PALETTE_H

// Tell tv.h which symbols we need.
#define Uses_TView
#define Uses_TWindow
#define Uses_TPalette
#define Uses_TPoint
#define Uses_TRect

#if __has_include(<tvision/tv.h>)
  #include <tvision/tv.h>
#elif __has_include(<tv.h>)
  #include <tv.h>
#else
  #error "Turbo Vision header not found: expected <tvision/tv.h> or <tv.h>"
#endif

class TTestView : public TView
{
public:
    explicit TTestView(TRect& r);
    ~TTestView() override = default;

    void draw() override;
    TPalette& getPalette() const override;
};

#define TEST_WIDTH  42
#define TEST_HEIGHT 11

class TTestWindow : public TWindow
{
public:
    TTestWindow();
    ~TTestWindow() override = default;

    TPalette& getPalette() const override;

    void sizeLimits(TPoint& min, TPoint& max) override
    {
        min.x = max.x = TEST_WIDTH;
        min.y = max.y = TEST_HEIGHT;
    }
};

#endif // _FOX_PALETTE_H
