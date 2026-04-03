#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "tv/foxtalk_layout.hpp"
#include "tv/foxtalk_redirect.hpp"
#include "tv/foxtalk_shell_bridge.hpp"
#include "tv/foxtalk_workspace_view.hpp"

// ---- TVision uses ----
#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TEvent
#define Uses_TMenuBar
#define Uses_TRect
#define Uses_TStatusLine
#define Uses_TBackground
#define Uses_TProgram
#define Uses_TScreen

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace xbase {
class DbArea;
}

namespace foxtalk {

class FoxtalkOutputWindow;
class FoxtalkCommandWindow;
class FoxtalkWorkspaceWindow;

class TFoxtalkApp : public TApplication, public IOutputSink {
public:
    TFoxtalkApp();
    ~TFoxtalkApp() override;

    static TMenuBar* initMenuBar(TRect bounds);
    static TStatusLine* initStatusLine(TRect bounds);
    static TDeskTop* initDeskTop(TRect bounds);

    void handleEvent(TEvent& event) override;
    void idle() override;

    void enqueueChunk(const std::string& s) override;

private:
    void createBackground();
    void createOutputWindow();
    void createCommandWindow();
    void createWorkspaceWindow();

    void showOutputWindow();
    void showWorkspaceWindow();
    void showMessage(const std::string& m);

    void runImmediate(const std::string& line);
    void runFromInput();
    void executeCommandLine(const std::string& line);

    void flushPending();

    void pushHistory(const std::string& s);
    void historyPrev();
    void historyNext();
    void setInputFromHistory();

    void loadState();
    void saveStateFromWindows();
    void applyStateToWindows();

    void toggleZoomCommandWindow();
    void resetCommandWindow();
    void toggleZoomOutputWindow();
    void resetOutputWindow();

    void refreshWorkspaceWindow();
    WorkspaceSnapshot makeWorkspaceSnapshot();

    bool dispatchMenu(int id);

    xbase::DbArea& curArea();

private:
    FoxtalkOutputWindow* outWin_{nullptr};
    FoxtalkCommandWindow* cmdWin_{nullptr};
    FoxtalkWorkspaceWindow* wsWin_{nullptr};

    RedirectController redirect_;
    ShellBridge shell_;
    LayoutState layout_;

    std::mutex qMutex_;
    std::vector<std::string> pending_;
    bool flushPosted_{false};

    int histPos_{0};
    static constexpr int kMaxHistory = 200;
};

} // namespace foxtalk