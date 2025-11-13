// src/cli/cmd_foxtalk.cpp — Launches FoxPro-style Turbo Vision UI for DotTalk.

#include <iostream>
#include <sstream>
#include "xbase.hpp"
#include "command_registry.hpp"
#include "textio.hpp"

// Turbo Vision preprocessor directives
#define Uses_TApplication
#define Uses_TDialog
#define Uses_TRect
#define Uses_TEvent
#define Uses_TMenuBar
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TStatusLine
#define Uses_TStatusItem
#define Uses_TStatusDef
#define Uses_TDeskTop
#define Uses_TInputLine
#define Uses_TButton
#define Uses_TKeys
#define Uses_TProgInit
#define Uses_TScreen
#define Uses_MsgBox
#include <tvision/tv.h>

const int cmFoxProCommand = 101;
const int cmFoxProExit = cmQuit;
const int cmDotTalkReplace = 106;
const int cmDotTalkList = 107;
const int cmDotTalkBrowse = 108;
const int cmDotTalkSeek = 109;
const int cmDotTalkAbout = 110;

class FDotTalkApp : public TApplication
{
public:
    FDotTalkApp();
    virtual void handleEvent(TEvent& event);
    static TMenuBar* initMenuBar(TRect r);
    static TStatusLine* initStatusLine(TRect r);
    void commandDialog(xbase::DbArea& area);
    void setDbArea(xbase::DbArea* area) { dbArea = area; }

private:
    xbase::DbArea* dbArea; // Reference to DbArea from cmd_FOXPRO
};

FDotTalkApp::FDotTalkApp() :
    TProgInit(&FDotTalkApp::initStatusLine,
              &FDotTalkApp::initMenuBar,
              &FDotTalkApp::initDeskTop),
    dbArea(nullptr)
{
    // Temporarily comment out TScreen usage due to API incompatibility
/*    
TScreen::setDefaultPalette({
        0x1F, // White on Blue for main window
        0x70, // Gray background for dialogs
        0x1E, // Yellow for highlights
        0x1F, // White on Blue for status line
});
*/
    
}

void FDotTalkApp::commandDialog(xbase::DbArea& area)
{
    TDialog* d = new TDialog(TRect(20, 6, 60, 18), "DotTalk Command");
    d->options |= ofCentered;

    TInputLine* input = new TInputLine(TRect(2, 2, 38, 3), 256);
    d->insert(input);

    d->insert(new TButton(TRect(28, 4, 38, 6), "~O~K", cmOK, bfDefault));
    d->insert(new TButton(TRect(28, 6, 38, 8), "~C~ancel", cmCancel, bfNormal));

    ushort result = deskTop->execView(d);
    if (result == cmOK) {
        char* cmd = input->data;
        std::string trimmed = cmd;
        if (!trimmed.empty()) {
            std::istringstream tok(trimmed);
            std::string cmdToken;
            tok >> cmdToken;
            if (!cmdToken.empty()) {
                std::string U = textio::up(cmdToken);
                if (!dli::registry().run(area, U, tok)) {
                    messageBox("Unknown command: " + cmdToken, mfError | mfOKButton);
                }
            }
        }
    }
    destroy(d);
}

void FDotTalkApp::handleEvent(TEvent& event)
{
    TApplication::handleEvent(event);
    if (event.what == evCommand && dbArea) {
        switch (event.message.command) {
            case cmFoxProCommand:
                commandDialog(*dbArea);
                clearEvent(event);
                break;
            default:
                break;
        }
    }
}

TMenuBar* FDotTalkApp::initMenuBar(TRect r)
{
    r.b.y = r.a.y + 1;
    return new TMenuBar(r,
        *new TSubMenu("~F~ile", kbAltF) +
            *new TMenuItem("~N~ew", cmNew, kbNoKey) +
            *new TMenuItem("~O~pen", cmOpen, kbNoKey) +
            *new TMenuItem("~C~lose", cmClose, kbNoKey) +
            newLine() +
            *new TMenuItem("E~x~it", cmFoxProExit, kbAltX, hcNoContext, "Alt-X") +
        *new TSubMenu("~E~dit", kbAltE) +
            *new TMenuItem("~C~opy", cmCopy, kbNoKey) +
            *new TMenuItem("~R~eplace", cmDotTalkReplace, kbNoKey) +
        *new TSubMenu("~D~atabase", kbAltD) +
            *new TMenuItem("~L~ist", cmDotTalkList, kbNoKey) +
            *new TMenuItem("~B~rowse", cmDotTalkBrowse, kbNoKey) +
            *new TMenuItem("~S~eek", cmDotTalkSeek, kbNoKey) +
        *new TSubMenu("~C~ommand", kbAltC) +
            *new TMenuItem("~E~nter Command...", cmFoxProCommand, kbAltE, hcNoContext, "Alt-E") +
        *new TSubMenu("~H~elp", kbAltH) +
            *new TMenuItem("~A~bout", cmDotTalkAbout, kbNoKey)
    );
}

TStatusLine* FDotTalkApp::initStatusLine(TRect r)
{
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmFoxProExit) +
            *new TStatusItem("~F10~ Menu", kbF10, cmMenu) +
            *new TStatusItem("~Alt-E~ Command", kbAltE, cmFoxProCommand)
    );
}

void cmd_FOXTALK(xbase::DbArea& area, std::istringstream& /*args*/)
{
#ifdef DOTTALK_TV_AVAILABLE
    std::cout << "Launching DotTalk FoxPro UI...\n";
    FDotTalkApp app;
    app.setDbArea(&area);
    app.run();
#else
    std::cout << "TVISION is not available in this build (library not found at configure time).\n";
    std::cout << "Tip: Install tvision (vcpkg or local), enable manifest mode/toolchain, re-configure, and rebuild.\n";
#endif
}