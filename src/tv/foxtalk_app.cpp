// ---- TVision uses ----
#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TMenuBar
#define Uses_TStatusLine
#define Uses_TBackground
#define Uses_TProgram
#define Uses_TScreen
#define Uses_TKeys
#define Uses_MsgBox

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

#include "tv/foxtalk_app.hpp"

#include <iostream>
#include <sstream>
#include <string>

#include "command_registry.hpp"
#include "tv/foxtalk_cmd_input.hpp"
#include "tv/foxtalk_command_window.hpp"
#include "tv/foxtalk_ids.hpp"
#include "tv/foxtalk_log_view.hpp"
#include "tv/foxtalk_menu.hpp"
#include "tv/foxtalk_output_window.hpp"
#include "tv/foxtalk_status.hpp"
#include "tv/foxtalk_util.hpp"
#include "tv/foxtalk_workspace_window.hpp"
#include "xbase.hpp"

// externs from your shell
extern "C" xbase::XBaseEngine* shell_engine();
extern "C" void register_shell_commands(xbase::XBaseEngine& eng, bool include_ui_cmds);

namespace foxtalk {

namespace {

static bool probeHasOpenTable(xbase::DbArea& area)
{
    return area.isOpen();
}

static std::string probeAlias(xbase::DbArea& area)
{
    return area.logicalName();
}

static std::string probeFileName(xbase::DbArea& area)
{
    return area.filename();
}

static long long probeRecno(xbase::DbArea& area)
{
    return static_cast<long long>(area.recno64());
}

static long long probeRecCount(xbase::DbArea& area)
{
    return static_cast<long long>(area.recCount64());
}

static bool probeDeleted(xbase::DbArea& area)
{
    return area.isDeleted();
}

static std::string probeOrderName(xbase::DbArea& area)
{
    (void)area;
    return {};
}

static std::string probeFilterText(xbase::DbArea& area)
{
    (void)area;
    return {};
}

} // anonymous namespace

TFoxtalkApp::TFoxtalkApp()
    : TProgInit(&TFoxtalkApp::initStatusLine,
                &TFoxtalkApp::initMenuBar,
                &TFoxtalkApp::initDeskTop),
      redirect_(this)
{
    loadState();

    createBackground();
    createOutputWindow();
    createCommandWindow();
    createWorkspaceWindow();

    static bool registered = false;
    if (!registered) {
        if (auto* eng = shell_engine())
            register_shell_commands(*eng, false);
        registered = true;
    }

    redirect_.install(OutputMode::ToWindow);
    redirect_.setMode(OutputMode::ToWindow);

    applyStateToWindows();
    refreshWorkspaceWindow();

    if (cmdWin_) {
        cmdWin_->show();
        cmdWin_->select();
        cmdWin_->focusInput();
    }

    showMessage("Alt-Z Cmd | Ctrl-F5 Cmd | Alt-O Out | Ctrl-F6 Out | F10 Menu | Alt-X Exit");
}

TFoxtalkApp::~TFoxtalkApp()
{
    saveStateFromWindows();
    saveLayoutState(layout_);
}

TMenuBar* TFoxtalkApp::initMenuBar(TRect bounds)
{
    return buildMenuBar(bounds);
}

TStatusLine* TFoxtalkApp::initStatusLine(TRect bounds)
{
    return buildStatusLine(bounds);
}

TDeskTop* TFoxtalkApp::initDeskTop(TRect bounds)
{
    return new TDeskTop(bounds);
}

void TFoxtalkApp::idle()
{
    TApplication::idle();
    std::cout.flush();
    std::cerr.flush();
    flushPending();
}

void TFoxtalkApp::enqueueChunk(const std::string& s)
{
    std::lock_guard<std::mutex> lk(qMutex_);
    pending_.push_back(s);

    if (!flushPosted_) {
        flushPosted_ = true;
        TEvent ev{};
        ev.what = evCommand;
        ev.message.command = cmFlushLog;
        putEvent(ev);
    }
}

void TFoxtalkApp::flushPending()
{
    std::vector<std::string> chunks;

    {
        std::lock_guard<std::mutex> lk(qMutex_);
        if (pending_.empty())
            return;
        chunks.swap(pending_);
        flushPosted_ = false;
    }

    if (outWin_ && outWin_->logView())
        outWin_->logView()->appendBatch(chunks);
}

void TFoxtalkApp::createBackground()
{
    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;
    deskTop->insert(new TBackground(TRect(S(0), S(0), S(sw), S(sh)), 0x20));
}

void TFoxtalkApp::createOutputWindow()
{
    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;

    clampGeometryToScreen(
        layout_.output,
        sw, sh,
        2, 2, sw - 4, sh - 9,
        20, 6
    );

    const TRect r(
        S(layout_.output.x),
        S(layout_.output.y),
        S(layout_.output.x + layout_.output.w),
        S(layout_.output.y + layout_.output.h)
    );

    outWin_ = new FoxtalkOutputWindow(r, "Foxtalk Output", 0);
    deskTop->insert(outWin_);
}

void TFoxtalkApp::createCommandWindow()
{
    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;

    clampGeometryToScreen(
        layout_.command,
        sw, sh,
        1, sh - 5, sw - 2, 4,
        20, 4
    );

    const TRect r(
        S(layout_.command.x),
        S(layout_.command.y),
        S(layout_.command.x + layout_.command.w),
        S(layout_.command.y + layout_.command.h)
    );

    cmdWin_ = new FoxtalkCommandWindow(r, "Command", 0);

    if (!layout_.lastInput.empty())
        cmdWin_->setInputText(layout_.lastInput);

    deskTop->insert(cmdWin_);
}

void TFoxtalkApp::createWorkspaceWindow()
{
    const TRect r(S(2), S(2), S(34), S(12));
    wsWin_ = new FoxtalkWorkspaceWindow(r, "Workspace", 0);
    deskTop->insert(wsWin_);
    refreshWorkspaceWindow();
}

void TFoxtalkApp::showOutputWindow()
{
    if (outWin_) {
        outWin_->show();
        outWin_->select();
        if (outWin_->logView()) {
            outWin_->logView()->select();
            outWin_->logView()->focus();
        }
    }
}

void TFoxtalkApp::showWorkspaceWindow()
{
    if (!wsWin_)
        return;

    refreshWorkspaceWindow();
    wsWin_->show();
    wsWin_->select();

    if (wsWin_->workspaceView()) {
        wsWin_->workspaceView()->select();
        wsWin_->workspaceView()->focus();
    }
}

void TFoxtalkApp::showMessage(const std::string& m)
{
    if (cmdWin_)
        cmdWin_->setMessage(m);
}

void TFoxtalkApp::runImmediate(const std::string& line)
{
    if (!cmdWin_)
        return;

    cmdWin_->setInputText(line);

    TEvent ev{};
    ev.what = evCommand;
    ev.message.command = cmRunCmd;
    putEvent(ev);
}

void TFoxtalkApp::runFromInput()
{
    if (!cmdWin_)
        return;

    const std::string s = trim(cmdWin_->getInputText());
    if (!s.empty())
        executeCommandLine(s);

    cmdWin_->focusInput();
}

void TFoxtalkApp::executeCommandLine(const std::string& line)
{
    const std::string trimmed = trim(line);
    if (trimmed.empty())
        return;

    pushHistory(trimmed);
    layout_.lastInput = trimmed;

    const std::string resolved = shell_.resolveShortcuts(trimmed);
    std::cout << "> " << resolved << "\n";

    {
        std::istringstream tok(resolved);
        std::string cmd;
        tok >> cmd;
        cmd = upcopy(cmd);

        if (cmd == "WIN") {
            std::string sub;
            tok >> sub;
            sub = upcopy(trim(sub));

            if (sub == "SAVE") {
                saveStateFromWindows();
                saveLayoutState(layout_);
                std::cout << "Window layout saved.\n";
                showMessage("WIN SAVE done.");
                refreshWorkspaceWindow();
                return;
            }

            if (sub == "RESTORE") {
                loadState();
                applyStateToWindows();
                std::cout << "Window layout restored.\n";
                showMessage("WIN RESTORE done.");
                refreshWorkspaceWindow();
                return;
            }

            if (sub == "DEFAULTS" || sub == "RESET") {
                resetCommandWindow();
                resetOutputWindow();
                saveStateFromWindows();
                saveLayoutState(layout_);
                std::cout << "Window layout reset to defaults.\n";
                showMessage("WIN DEFAULTS done.");
                refreshWorkspaceWindow();
                return;
            }

            std::cout << "Usage: WIN SAVE | WIN RESTORE | WIN DEFAULTS\n";
            showMessage("WIN usage shown.");
            refreshWorkspaceWindow();
            return;
        }
    }

    const ShellRunResult result = shell_.runLine(curArea(), trimmed);
    if (!result.statusMessage.empty())
        showMessage(result.statusMessage);

    refreshWorkspaceWindow();
}

void TFoxtalkApp::pushHistory(const std::string& s)
{
    if (layout_.history.empty() || layout_.history.back() != s)
        layout_.history.push_back(s);

    if (static_cast<int>(layout_.history.size()) > kMaxHistory) {
        layout_.history.erase(
            layout_.history.begin(),
            layout_.history.begin() + (static_cast<int>(layout_.history.size()) - kMaxHistory)
        );
    }

    histPos_ = static_cast<int>(layout_.history.size());
}

void TFoxtalkApp::historyPrev()
{
    if (layout_.history.empty())
        return;

    if (histPos_ > 0)
        --histPos_;

    setInputFromHistory();
}

void TFoxtalkApp::historyNext()
{
    if (layout_.history.empty())
        return;

    if (histPos_ < static_cast<int>(layout_.history.size()))
        ++histPos_;

    setInputFromHistory();
}

void TFoxtalkApp::setInputFromHistory()
{
    if (!cmdWin_)
        return;

    std::string txt;
    if (histPos_ >= 0 && histPos_ < static_cast<int>(layout_.history.size()))
        txt = layout_.history[static_cast<std::size_t>(histPos_)];

    cmdWin_->setInputText(txt);
    cmdWin_->focusInput();

    if (histPos_ >= 0 && histPos_ < static_cast<int>(layout_.history.size()))
        showMessage("History: " + txt);
    else
        showMessage("History end.");
}

void TFoxtalkApp::loadState()
{
    loadLayoutState(layout_, kMaxHistory);
    histPos_ = static_cast<int>(layout_.history.size());
}

void TFoxtalkApp::saveStateFromWindows()
{
    if (outWin_) {
        layout_.output.x = outWin_->origin.x;
        layout_.output.y = outWin_->origin.y;
        layout_.output.w = outWin_->size.x;
        layout_.output.h = outWin_->size.y;
        layout_.output.zoomed = false;
    }

    if (cmdWin_) {
        layout_.command.x = cmdWin_->origin.x;
        layout_.command.y = cmdWin_->origin.y;
        layout_.command.w = cmdWin_->size.x;
        layout_.command.h = cmdWin_->size.y;
        layout_.command.zoomed = false;
    }
}

void TFoxtalkApp::applyStateToWindows()
{
    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;

    clampGeometryToScreen(
        layout_.output,
        sw, sh,
        2, 2, sw - 4, sh - 9,
        20, 6
    );

    clampGeometryToScreen(
        layout_.command,
        sw, sh,
        1, sh - 5, sw - 2, 4,
        20, 4
    );

    if (outWin_) {
        TRect outRect(
            S(layout_.output.x),
            S(layout_.output.y),
            S(layout_.output.x + layout_.output.w),
            S(layout_.output.y + layout_.output.h)
        );
        outWin_->locate(outRect);
    }

    if (cmdWin_) {
        TRect cmdRect(
            S(layout_.command.x),
            S(layout_.command.y),
            S(layout_.command.x + layout_.command.w),
            S(layout_.command.y + layout_.command.h)
        );
        cmdWin_->locate(cmdRect);
        cmdWin_->focusInput();
    }
}

void TFoxtalkApp::toggleZoomCommandWindow()
{
    if (!cmdWin_)
        return;

    message(cmdWin_, evCommand, cmZoom, cmdWin_);
    cmdWin_->focusInput();
}

void TFoxtalkApp::resetCommandWindow()
{
    if (!cmdWin_)
        return;

    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;

    TRect r(S(1), S(sh - 5), S(sw - 1), S(sh - 1));
    cmdWin_->locate(r);
    cmdWin_->flags |= (wfMove | wfGrow | wfZoom);
    cmdWin_->select();
    cmdWin_->focusInput();
    showMessage("Command window restored.");
}

void TFoxtalkApp::toggleZoomOutputWindow()
{
    if (!outWin_)
        return;

    message(outWin_, evCommand, cmZoom, outWin_);
    if (outWin_->logView()) {
        outWin_->logView()->select();
        outWin_->logView()->focus();
    }
}

void TFoxtalkApp::resetOutputWindow()
{
    if (!outWin_)
        return;

    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;

    TRect r(S(2), S(2), S(sw - 2), S(sh - 7));
    outWin_->locate(r);
    outWin_->flags |= (wfMove | wfGrow | wfZoom);
    outWin_->select();

    if (outWin_->logView()) {
        outWin_->logView()->select();
        outWin_->logView()->focus();
    }

    showMessage("Output window restored.");
}

void TFoxtalkApp::refreshWorkspaceWindow()
{
    if (!wsWin_)
        return;

    wsWin_->setSnapshot(makeWorkspaceSnapshot());
}

WorkspaceSnapshot TFoxtalkApp::makeWorkspaceSnapshot()
{
    WorkspaceSnapshot s;

    auto* eng = shell_engine();
    if (!eng)
        return s;

    s.areaNumber = eng->currentArea();

    xbase::DbArea& area = eng->area(s.areaNumber);

    s.hasOpenTable = probeHasOpenTable(area);
    if (!s.hasOpenTable)
        return s;

    s.alias      = probeAlias(area);
    s.fileName   = probeFileName(area);
    s.recno      = probeRecno(area);
    s.recCount   = probeRecCount(area);
    s.deleted    = probeDeleted(area);
    s.orderName  = probeOrderName(area);
    s.filterText = probeFilterText(area);

    return s;
}

bool TFoxtalkApp::dispatchMenu(int id)
{
    switch (id) {
        case cmFileOpen:       runImmediate("USE"); return true;
        case cmFileClose:      runImmediate("CLOSE"); return true;
        case cmAreaSelect:     runImmediate("AREA"); return true;
        case cmIndexOpen:      runImmediate("SET INDEX"); return true;
        case cmIndexClose:     runImmediate("SET INDEX TO"); return true;
        case cmWinSave:        runImmediate("WIN SAVE"); return true;
        case cmWinRestore:     runImmediate("WIN RESTORE"); return true;
        case cmWinDefaults:    runImmediate("WIN DEFAULTS"); return true;

        case cmBrowse:         runImmediate("BROWSE"); return true;
        case cmRecordView:     runImmediate("RECORDVIEW"); return true;
        case cmList:           runImmediate("LIST"); return true;
        case cmSmartList:      runImmediate("SMARTLIST"); return true;
        case cmCount:          runImmediate("COUNT"); return true;
        case cmFind:           runImmediate("FIND"); return true;
        case cmLocate:         runImmediate("LOCATE"); return true;
        case cmWhere:          runImmediate("WHERE"); return true;
        case cmWhereClear:     runImmediate("WHERE"); return true;

        case cmWorkspace:
            showWorkspaceWindow();
            return true;

        case cmShowDeletedStub:
            messageBox("TODO: Toggle 'SET DELETED ON/OFF'", mfInformation);
            return true;

        case cmStatusStub:
            messageBox("TODO: STATUS summary window", mfInformation);
            return true;

        case cmSeek:           runImmediate("SEEK"); return true;
        case cmSetOrder:       runImmediate("SET ORDER"); return true;
        case cmReindex:        runImmediate("REINDEX"); return true;

        case cmTagManagerStub:
            messageBox("TODO: Tag Manager", mfInformation);
            return true;

        case cmOrderInfoStub:
            messageBox("TODO: Order Info", mfInformation);
            return true;

        case cmSetRelation:    runImmediate("SET RELATION"); return true;
        case cmRelRefresh:     runImmediate("RELREFRESH"); return true;
        case cmRelClear:       runImmediate("CLEAR RELATION"); return true;

        case cmRelBrowserStub:
            messageBox("TODO: Relations Browser", mfInformation);
            return true;

        case cmAppend:         runImmediate("APPEND"); return true;
        case cmEdit:           runImmediate("EDIT"); return true;
        case cmDelete:         runImmediate("DELETE"); return true;
        case cmRecall:         runImmediate("RECALL"); return true;
        case cmPack:           runImmediate("PACK"); return true;
        case cmTurboPack:      runImmediate("TURBOPACK"); return true;

        case cmZapStub:
            messageBox("TODO: ZAP", mfInformation);
            return true;

        case cmBlankStub:
            messageBox("TODO: BLANK", mfInformation);
            return true;

        case cmCalc:           runImmediate("CALC"); return true;
        case cmEval:           runImmediate("EVALUATE"); return true;
        case cmHelp:           runImmediate("HELP"); return true;
        case cmSelfTest:       runImmediate("/SELFTEST 300"); return true;

        case cmHistorySizeStub:
            messageBox("TODO: History Size", mfInformation);
            return true;

        case cmVerbosityStub:
            messageBox("TODO: Message Verbosity", mfInformation);
            return true;

        case cmOutputModeStub:
            messageBox("TODO: Output Mode", mfInformation);
            return true;

        case cmPaletteColor:   runImmediate("COLOR"); return true;

        case cmKeysStub:
            messageBox("TODO: Keys Cheat Sheet", mfInformation);
            return true;

        case cmAboutStub:
            messageBox("Foxtalk (DotTalk++) - TVision shell.\n(c) 2025", mfInformation);
            return true;
    }

    return false;
}

void TFoxtalkApp::handleEvent(TEvent& event)
{
    TApplication::handleEvent(event);

    if (event.what == evCommand && event.message.command == cmFlushLog) {
        flushPending();
        clearEvent(event);
        return;
    }

    if (event.what == evKeyDown) {
        if (event.keyDown.keyCode == kbCtrlQ) {
            if (cmdWin_) {
                cmdWin_->show();
                cmdWin_->select();
                cmdWin_->focusInput();
            }
            clearEvent(event);
            return;
        }

        if (event.keyDown.keyCode == kbF2) {
            showOutputWindow();
            clearEvent(event);
            return;
        }

        if (event.keyDown.keyCode == kbF3) {
            runImmediate("RECORDVIEW");
            clearEvent(event);
            return;
        }

        if (event.keyDown.keyCode == kbF4) {
            showWorkspaceWindow();
            clearEvent(event);
            return;
        }

        if (event.keyDown.keyCode == kbAltX) {
            event.what = evCommand;
            event.message.command = cmTalkExit;
        }

        if (event.keyDown.keyCode == kbAltZ) {
            event.what = evCommand;
            event.message.command = cmZoomCmd;
        }

        if (event.keyDown.keyCode == kbCtrlF5) {
            event.what = evCommand;
            event.message.command = cmResetCmdWin;
        }

        if (event.keyDown.keyCode == kbAltO) {
            event.what = evCommand;
            event.message.command = cmZoomOut;
        }

        if (event.keyDown.keyCode == kbCtrlF6) {
            event.what = evCommand;
            event.message.command = cmResetOutWin;
        }

        if (cmdWin_ && cmdWin_->input() && cmdWin_->input()->getState(sfFocused)) {
            if (event.keyDown.keyCode == kbUp) {
                historyPrev();
                clearEvent(event);
                return;
            }
            if (event.keyDown.keyCode == kbDown) {
                historyNext();
                clearEvent(event);
                return;
            }
        }
    }

    if (event.what == evCommand) {
        const int id = event.message.command;

        if (id == cmShowOutput) {
            showOutputWindow();
            clearEvent(event);
            return;
        }

        if (id == cmWorkspace) {
            showWorkspaceWindow();
            clearEvent(event);
            return;
        }

        if (id == cmRunCmd) {
            runFromInput();
            clearEvent(event);
            return;
        }

        if (id == cmFocusCmd) {
            if (cmdWin_) {
                cmdWin_->show();
                cmdWin_->select();
                cmdWin_->focusInput();
            }
            clearEvent(event);
            return;
        }

        if (id == cmZoomCmd) {
            toggleZoomCommandWindow();
            clearEvent(event);
            return;
        }

        if (id == cmResetCmdWin) {
            resetCommandWindow();
            clearEvent(event);
            return;
        }

        if (id == cmZoomOut) {
            toggleZoomOutputWindow();
            clearEvent(event);
            return;
        }

        if (id == cmResetOutWin) {
            resetOutputWindow();
            clearEvent(event);
            return;
        }

        if (id == cmTalkExit) {
            message(this, evCommand, cmQuit, this);
            clearEvent(event);
            return;
        }

        if (dispatchMenu(id)) {
            clearEvent(event);
            return;
        }
    }
}

xbase::DbArea& TFoxtalkApp::curArea()
{
    auto* e = shell_engine();
    return e->area(e->currentArea());
}

} // namespace foxtalk