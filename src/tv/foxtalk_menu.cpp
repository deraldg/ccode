// ---- TVision uses ----
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TMenu
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TKeys

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

#include "tv/foxtalk_menu.hpp"
#include "tv/foxtalk_ids.hpp"
#include "tv/foxtalk_util.hpp"

namespace foxtalk {

TMenuBar* buildMenuBar(TRect bounds)
{
    bounds.b.y = S(bounds.a.y + 1);

    TMenu* root = new TMenu(
        *new TSubMenu("~F~ile", kbAltF)
            + *new TMenuItem("~O~pen Table...", cmFileOpen,  kbNoKey, hcNoContext, "USE")
            + *new TMenuItem("~C~lose Table",    cmFileClose, kbNoKey, hcNoContext, "CLOSE")
            + *new TMenuItem("~S~elect Area...", cmAreaSelect, kbNoKey, hcNoContext, "AREA")
            + *new TMenuItem("~O~pen Index...",  cmIndexOpen,  kbNoKey, hcNoContext, "SET INDEX")
            + *new TMenuItem("C~l~ose Indexes",  cmIndexClose, kbNoKey, hcNoContext, "SET INDEX TO")
            + newLine()
            + *new TMenuItem("Save ~L~ayout",    cmWinSave,     kbNoKey, hcNoContext, "WIN SAVE")
            + *new TMenuItem("~R~estore Layout", cmWinRestore,  kbNoKey, hcNoContext, "WIN RESTORE")
            + *new TMenuItem("~D~efaults (Reset Windows)", cmWinDefaults, kbNoKey, hcNoContext, "WIN DEFAULTS")
            + newLine()
            + *new TMenuItem("E~x~it", cmTalkExit, kbAltX, hcNoContext, "Alt-X")

      + *new TSubMenu("~V~iew", kbAltV)
            + *new TMenuItem("~B~rowse (Super)", cmBrowse,     kbNoKey, hcNoContext, "BROWSE")
            + *new TMenuItem("~R~ecord View",    cmRecordView, kbF3,    hcNoContext, "RECORDVIEW")
            + newLine()
            + *new TMenuItem("~L~ist",           cmList,      kbNoKey, hcNoContext, "LIST")
            + *new TMenuItem("~S~martList",      cmSmartList, kbNoKey, hcNoContext, "SMARTLIST")
            + *new TMenuItem("~C~ount",          cmCount,     kbNoKey, hcNoContext, "COUNT")
            + newLine()
            + *new TMenuItem("~F~ind...",        cmFind,      kbNoKey, hcNoContext, "FIND")
            + *new TMenuItem("~L~ocate...",      cmLocate,    kbNoKey, hcNoContext, "LOCATE FOR")
            + *new TMenuItem("~W~HERE / Filter...", cmWhere,  kbNoKey, hcNoContext, "WHERE")
            + *new TMenuItem("~C~lear Filter",   cmWhereClear, kbNoKey, hcNoContext, "WHERE")
            + newLine()
            + *new TMenuItem("Show ~D~eleted (Toggle) [STUB]", cmShowDeletedStub, kbNoKey, hcNoContext, "")
            + *new TMenuItem("Show ~S~tatus [STUB]",           cmStatusStub,      kbNoKey, hcNoContext, "")

      + *new TSubMenu("~I~ndex", kbAltI)
            + *new TMenuItem("~S~eek...",            cmSeek,     kbNoKey, hcNoContext, "SEEK")
            + *new TMenuItem("Set ~O~rder / Tag...", cmSetOrder, kbNoKey, hcNoContext, "SET ORDER")
            + *new TMenuItem("Re~i~ndex / Update",   cmReindex,  kbNoKey, hcNoContext, "REINDEX")
            + newLine()
            + *new TMenuItem("~T~ag Manager... [STUB]", cmTagManagerStub, kbNoKey, hcNoContext, "")
            + *new TMenuItem("Order ~I~nfo... [STUB]",  cmOrderInfoStub,  kbNoKey, hcNoContext, "")

      + *new TSubMenu("~R~elations", kbAltR)
            + *new TMenuItem("Set ~R~elation To...", cmSetRelation, kbNoKey, hcNoContext, "SET RELATION")
            + *new TMenuItem("~R~efresh Child",      cmRelRefresh, kbNoKey, hcNoContext, "RELREFRESH")
            + *new TMenuItem("~C~lear Relations",    cmRelClear,   kbNoKey, hcNoContext, "CLEAR RELATION")
            + newLine()
            + *new TMenuItem("Relations ~B~rowser... [STUB]", cmRelBrowserStub, kbNoKey, hcNoContext, "")

      + *new TSubMenu("~E~dit/Records", kbAltE)
            + *new TMenuItem("~A~ppend", cmAppend, kbNoKey, hcNoContext, "APPEND")
            + *new TMenuItem("~E~dit",   cmEdit,   kbNoKey, hcNoContext, "EDIT")
            + *new TMenuItem("~D~elete", cmDelete, kbNoKey, hcNoContext, "DELETE")
            + *new TMenuItem("~R~ecall", cmRecall, kbNoKey, hcNoContext, "RECALL")
            + newLine()
            + *new TMenuItem("~P~ack",      cmPack,      kbNoKey, hcNoContext, "PACK")
            + *new TMenuItem("T~U~RBOPACK", cmTurboPack, kbNoKey, hcNoContext, "TURBOPACK")
            + newLine()
            + *new TMenuItem("~Z~ap [STUB]",          cmZapStub,   kbNoKey, hcNoContext, "")
            + *new TMenuItem("~B~lank Record [STUB]", cmBlankStub, kbNoKey, hcNoContext, "")

      + *new TSubMenu("~T~ools", kbAltT)
            + *new TMenuItem("~C~alculator / CALC...", cmCalc, kbNoKey, hcNoContext, "CALC")
            + *new TMenuItem("~E~valuate...",          cmEval, kbNoKey, hcNoContext, "EVALUATE")
            + *new TMenuItem("~H~elp (Commands)",      cmHelp, kbNoKey, hcNoContext, "HELP")
            + *new TMenuItem("~S~elf-Test (Output Flood)", cmSelfTest, kbNoKey, hcNoContext, "/SELFTEST")
            + newLine()
            + *new TMenuItem("~H~istory Size... [STUB]",      cmHistorySizeStub, kbNoKey, hcNoContext, "")
            + *new TMenuItem("Message ~V~erbosity... [STUB]", cmVerbosityStub,   kbNoKey, hcNoContext, "")
            + *new TMenuItem("~O~utput Mode [STUB]",          cmOutputModeStub,  kbNoKey, hcNoContext, "")
            + *new TMenuItem("~C~olors / Palette...",         cmPaletteColor,    kbNoKey, hcNoContext, "COLOR")

      + *new TSubMenu("~W~indow", kbAltW)
            + *new TMenuItem("~O~utput",      cmShowOutput, kbF2,    hcNoContext, "F2")
            + *new TMenuItem("~C~ommand Bar", cmFocusCmd,   kbCtrlQ, hcNoContext, "Ctrl-Q")
            + *new TMenuItem("~W~orkspace",   cmWorkspace,  kbF4,    hcNoContext, "F4")
            + newLine()
            + *new TMenuItem("~Z~oom Command",    cmZoomCmd,     kbAltZ,   hcNoContext, "Alt-Z")
            + *new TMenuItem("~R~estore Command", cmResetCmdWin, kbCtrlF5, hcNoContext, "Ctrl-F5")
            + *new TMenuItem("Z~o~om Output",     cmZoomOut,     kbAltO,   hcNoContext, "Alt-O")
            + *new TMenuItem("R~e~store Output",  cmResetOutWin, kbCtrlF6, hcNoContext, "Ctrl-F6")

      + *new TSubMenu("~H~elp", kbAltH)
            + *new TMenuItem("~H~elp",                    cmHelp,     kbNoKey, hcNoContext, "HELP")
            + *new TMenuItem("~K~eys Cheat Sheet [STUB]", cmKeysStub, kbNoKey, hcNoContext, "")
            + *new TMenuItem("~A~bout Foxtalk... [STUB]", cmAboutStub, kbNoKey, hcNoContext, "")
    );

    return new TMenuBar(bounds, root);
}

} // namespace foxtalk