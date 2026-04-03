#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE)
// src/cli/palette_app.cpp
#define Uses_TKeys
#define Uses_TRect
#define Uses_TEvent
#define Uses_TMenuBar
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TDeskTop
#define Uses_TApplication
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_MsgBox
#include <tvision/tv.h>
#include "cmds.h"
#include "palette_app.h"
#include "palette.h"

#define cpPaletteAppC "\x3E\x2D\x72\x5F\x68\x4E"
#define cpPaletteAppBW "\x07\x07\x0F\x70\x78\x7F"
#define cpPaletteAppM "\x07\x0F\x70\x09\x0F\x79"

TPaletteApp::TPaletteApp() :
    TProgInit(initStatusLine, initMenuBar, initDeskTop)
{
}

TMenuBar *TPaletteApp::initMenuBar(TRect bounds)
{
    bounds.b.y = bounds.a.y + 1;
    return new TMenuBar(bounds, new TMenu(
        *new TMenuItem("~A~bout...", cmAbout, kbAltA, hcNoContext, 0,
         new TMenuItem("~P~alette", cmPaletteView, kbAltP, hcNoContext, 0,
         new TMenuItem("E~x~it", cmQuit, kbAltX)))
    ));
}

TStatusLine *TPaletteApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
        *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit) +
        *new TStatusItem(0, kbF10, cmMenu)
    );
}

void TPaletteApp::handleEvent(TEvent& event)
{
    TApplication::handleEvent(event);
    if (event.what == evCommand)
    {
        switch (event.message.command)
        {
            case cmAbout:
                aboutDlg();
                clearEvent(event);
                break;
            case cmPaletteView:
                paletteView();
                clearEvent(event);
                break;
            default:
                break;
        }
    }
}

TPalette& TPaletteApp::getPalette() const
{
    static TPalette
        newColor(cpPaletteAppC, sizeof(cpPaletteAppC) - 1),
        newBlackWhite(cpPaletteAppBW, sizeof(cpPaletteAppBW) - 1),
        newMonochrome(cpPaletteAppM, sizeof(cpPaletteAppM) - 1);
    static TPalette *palettes[] =
    {
        &newColor,
        &newBlackWhite,
        &newMonochrome
    };
    return *(palettes[appPalette]);
}

void TPaletteApp::aboutDlg()
{
    TDialog *aboutDlgBox = new TDialog(TRect(0, 0, 47, 13), "About");
    if (validView(aboutDlgBox))
    {
        aboutDlgBox->insert(
            new TStaticText(
                TRect(2, 1, 45, 9),
                "\n\003PALETTE EXAMPLE\n \n"
                "\003A Turbo Vision Demo\n \n"
                "\003written by\n \n"
                "\003Borland C++ Tech Support\n"
            ));
        aboutDlgBox->insert(
            new TButton(TRect(18, 10, 29, 12), "OK", cmOK, bfDefault)
        );
        aboutDlgBox->options |= ofCentered;
        execView(aboutDlgBox);
        destroy(aboutDlgBox);
    }
}

void TPaletteApp::paletteView()
{
    TView *view = new TTestWindow;
    if (validView(view))
        deskTop->insert(view);
}
#else
// TVision not enabled: implementation excluded.

#endif  // TVISION guard
