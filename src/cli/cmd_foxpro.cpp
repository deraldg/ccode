// src/cli/cmd_foxpro.cpp — FOXPRO-like Turbo Vision UI for DotTalk++
// - No App subclassing: we keep TApplication directly (avoids TProgInit ctor issues).
// - FoxPro MS-DOS colors via custom MenuBar/StatusLine palettes.
// - CoutRedirectBuf declared before use; Uses_TMenu added.
// - Width math mostly in 'int' (two benign C4244s may remain).

#include <iostream>
#include <sstream>
#include <streambuf>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cctype>
#include <fstream>
#include <cstdlib>

#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#  include <windows.h>
#endif

#include "xbase.hpp"
#include "command_registry.hpp"
#include "textio.hpp"

#define Uses_TApplication
#define Uses_TDialog
#define Uses_TScreen
#define Uses_TRect
#define Uses_TEvent
#define Uses_TMenu
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
#define Uses_TWindow
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TDrawBuffer
#define Uses_TBackground
#define Uses_MsgBox
#define Uses_TProgram
#include <tvision/tv.h>

// Default FoxPro palette ON unless disabled at build.
#ifndef DOTTALK_FOXPRO_PALETTE
#  define DOTTALK_FOXPRO_PALETTE 1
#endif

// -----------------------------------------------------------------------------
// DOS attribute helpers used for the FoxPro theme
// -----------------------------------------------------------------------------
using uchar = unsigned char;
static inline uchar _Attr(uchar bg, uchar fg) { return (uchar)((bg << 4) | (fg & 0x0F)); }

static inline uchar kWhtOnBlue() { return _Attr(1,15); } // 0x1F
static inline uchar kYelOnBlue() { return _Attr(1,14); } // 0x1E
static inline uchar kGryOnBlue() { return _Attr(1, 7); } // 0x17

static inline uchar kYelOnRed()  { return _Attr(4,14); } // 0x4E
static inline uchar kWhtOnRed()  { return _Attr(4,15); } // 0x4F

static inline uchar kBlkOnCyan() { return _Attr(3, 0); } // 0x30 (optional: log text)

// ---------- helpers ----------
static inline short S(int v) { return static_cast<short>(v); }
static std::string toLower(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (unsigned char c : s) o.push_back((char)std::tolower(c));
    return o;
}
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ---------- external shell bindings ----------
extern "C" xbase::XBaseEngine* shell_engine();
extern "C" void register_shell_commands(xbase::XBaseEngine& eng, bool include_ui_cmds);

// ---------- command IDs ----------
static const int cmFoxProExit     = cmQuit;
static const int cmShowOutput     = 201;
static const int cmModeWindow     = 202;
static const int cmModeConsole    = 203;
static const int cmModeBoth       = 204;
static const int cmFlushLog       = 205;
static const int cmFoxProCommand  = 206;

// FoxPro-like actions
enum : int {
    cmUse = 300, cmCloseFile, cmSelect, cmArea,
    cmBrowse, cmDisplay, cmFields, cmStruct, cmLocate, cmPack, cmZap,
    cmFind, cmReplace, cmSeek, cmSetOrder
#if DOTTALK_WITH_INDEX
    , cmAscend, cmDescend
#endif
};

// Bottom command bar actions
enum : int { cmRunCmd = 400, cmFocusCmd };

// ---------- modes ----------
enum class OutputMode { ToWindow, ToConsole, ToBoth };

// ---------- INI helper ----------
struct UiIni {
    std::string path;
    std::string lastCmd;
    OutputMode mode{OutputMode::ToWindow};
    int outX{2}, outY{3}, outW{78}, outH{18};
    int cmdX{0}, cmdY{0}, cmdW{0}, cmdH{0};

    static std::string appdataPath() {
#ifdef _WIN32
        const char* a = std::getenv("APPDATA");
        if (a && *a) return std::string(a) + "\\DotTalk";
        return ".";
#else
        const char* home = std::getenv("HOME");
        return (home && *home) ? (std::string(home) + "/.config/dottalk") : ".";
#endif
    }
    static bool ensureDir(const std::string&) { return true; }

    void load() {
        std::ifstream f(path);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            auto p = line.find('=');
            if (p == std::string::npos) continue;
            std::string k = trim(line.substr(0, p));
            std::string v = trim(line.substr(p + 1));
            if (k == "lastCmd") lastCmd = v;
            else if (k == "mode") {
                if (v == "console") mode = OutputMode::ToConsole;
                else if (v == "both") mode = OutputMode::ToBoth;
                else mode = OutputMode::ToWindow;
            } else if (k == "outX") outX = std::atoi(v.c_str());
            else if (k == "outY") outY = std::atoi(v.c_str());
            else if (k == "outW") outW = std::atoi(v.c_str());
            else if (k == "outH") outH = std::atoi(v.c_str());
            else if (k == "cmdX") cmdX = std::atoi(v.c_str());
            else if (k == "cmdY") cmdY = std::atoi(v.c_str());
            else if (k == "cmdW") cmdW = std::atoi(v.c_str());
            else if (k == "cmdH") cmdH = std::atoi(v.c_str());
        }
    }
    void save() const {
        std::ofstream f(path, std::ios::trunc);
        if (!f) return;
        f << "lastCmd=" << lastCmd << "\n";
        f << "mode=" << (mode == OutputMode::ToWindow ? "window" :
                         mode == OutputMode::ToConsole ? "console" : "both") << "\n";
        f << "outX=" << outX << "\n"
          << "outY=" << outY << "\n"
          << "outW=" << outW << "\n"
          << "outH=" << outH << "\n";
        f << "cmdX=" << cmdX << "\n"
          << "cmdY=" << cmdY << "\n"
          << "cmdW=" << cmdW << "\n"
          << "cmdH=" << cmdH << "\n";
    }
};

// ---------- forward decl ----------
class TDotTalkApp;

// ---------- iostream redirect (declare before TDotTalkApp uses it) ----------
class CoutRedirectBuf : public std::streambuf {
public:
    CoutRedirectBuf(class TDotTalkApp* app, std::streambuf* original, OutputMode mode)
        : app_(app), original_(original), mode_(mode) { setp(buf_, buf_ + sizeof(buf_) - 1); }
    void setMode(OutputMode m) { mode_ = m; }
protected:
    int sync() override { flushBuffer(); return 0; }
    int overflow(int ch) override {
        if (ch != EOF) { *pptr() = (char)ch; pbump(1); }
        if (ch == '\n') { flushBuffer(); return ch; }
        if (pptr() >= epptr()) flushBuffer();
        return ch;
    }
private:
    void flushBuffer();
    class TDotTalkApp*   app_;
    std::streambuf* original_;
    OutputMode     mode_;
    char           buf_[64 * 1024];
};

// ---------- Output scroller ----------
class TLogView : public TScroller {
public:
    explicit TLogView(const TRect& r, TScrollBar* h, TScrollBar* v)
        : TScroller(r, h, v) {
        options |= ofFramed;
        growMode = gfGrowHiX | gfGrowHiY;
        setLimit(S(1), S(1));
    }
    void setMaxLines(size_t maxLines) { maxLines_ = std::max<size_t>(1024, maxLines); }
    void appendBatch(const std::vector<std::string>& chunks) {
        for (const auto& s : chunks) appendInternal(s);
        finalizeAppend();
    }
    void clearAll() {
        lines_.clear(); maxWidth_ = 0;
        setLimit(S(1), S(1));
        scrollTo(S(0), S(0));
        drawView();
    }
    int findNext(const std::string& term, int fromRow, bool caseInsensitive) {
        if (term.empty() || lines_.empty()) return -1;
        const int n = (int)lines_.size();
        int i = std::max(0, fromRow);
        std::string needle = caseInsensitive ? toLower(term) : term;
        for (int k = 0; k < n; ++k) {
            int row = (i + k) % n;
            const std::string& src = lines_[(size_t)row];
            bool hit = caseInsensitive
                ? (toLower(src).find(needle) != std::string::npos)
                : (src.find(needle) != std::string::npos);
            if (hit) {
                scrollTo(S(0), S(std::max(0, row)));
                drawView();
                return row;
            }
        }
        return -1;
    }
    void draw() override {
        TDrawBuffer b;
#if DOTTALK_FOXPRO_PALETTE
        const ushort color = (ushort)kBlkOnCyan(); // FoxPro-style: black on cyan
#else
        const ushort color = getColor(0x0301);
#endif
        for (short row = 0; row < size.y; ++row) {
            const int idx = delta.y + row;
            std::string text = (idx >= 0 && idx < (int)lines_.size()) ? lines_[(size_t)idx] : std::string();

            const int sx = (int)size.x;
            int tlen = (int)text.size();
            if (tlen < sx) text.append((size_t)(sx - tlen), ' ');
            else if (tlen > sx) text.resize((size_t)sx);

            b.moveStr(0, text.c_str(), color);
            writeLine(S(0), row, size.x, S(1), b);
        }
    }
private:
    void appendInternal(const std::string& s) {
        size_t start = 0;
        while (start <= s.size()) {
            size_t nl = s.find('\n', start);
            std::string line = (nl == std::string::npos) ? s.substr(start) : s.substr(start, nl - start);
            if (!line.empty() || nl != std::string::npos) {
                lines_.push_back(line);
                while (lines_.size() > maxLines_) lines_.pop_front();
                const short w = (short)line.size();
                if (w > maxWidth_) maxWidth_ = w;
            }
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }
    void finalizeAppend() {
        const short limX = std::max<short>(S(1), maxWidth_);
        const short limY = std::max<short>(S(1), (short)lines_.size());
        setLimit(limX, limY);
        scrollTo(S(0), std::max<short>(S(0), (short)lines_.size() - size.y));
        drawView();
    }
    std::deque<std::string> lines_;
    size_t maxLines_{100000};
    short  maxWidth_{0};
};

// ---------- output window wrapper ----------
class TOutputWindow : public TWindow {
public:
    TOutputWindow(const TRect& r, const char* title, ushort n, TDotTalkApp* owner)
        : TWindow(r, title, n)
        , TWindowInit(&TWindow::initFrame)
        , owner_(owner) {}
    void close() override;
private:
    TDotTalkApp* owner_;
};

// ---------- bottom command input ----------
class TCmdInput : public TInputLine {
public:
    TCmdInput(const TRect& r, ushort maxLen) : TInputLine(r, maxLen) {}
    void prefill(const std::string& s) { setData((void*)s.c_str()); selectAll(True); }
    void handleEvent(TEvent& ev) override {
        TInputLine::handleEvent(ev);
        if (ev.what == evKeyDown && ev.keyDown.keyCode == kbEnter) {
            message(owner, evCommand, cmRunCmd, this);
            clearEvent(ev);
        }
    }
};

// ---------- AppBase (no subclassing) ----------
using AppBase = TApplication;

// ---------- FoxPro-styled widgets ----------
#if DOTTALK_FOXPRO_PALETTE
class FoxMenuBar : public TMenuBar {
public:
    FoxMenuBar(const TRect& r, TMenu* menu) : TMenuBar(r, menu) {}
    TPalette& getPalette() const override {
        static TColorAttr pal[32];
        static bool init = false;
        if (!init) {
            for (int i=0;i<32;++i) pal[i] = TColorAttr(kWhtOnBlue());
            pal[0] = TColorAttr(kWhtOnBlue()); // normal
            pal[1] = TColorAttr(kGryOnBlue()); // disabled
            pal[2] = TColorAttr(kYelOnBlue()); // hotkey
            pal[3] = TColorAttr(kYelOnBlue()); // selected
            pal[5] = TColorAttr(kWhtOnBlue()); // submenu normal
            pal[6] = TColorAttr(kYelOnBlue()); // submenu selected
            pal[7] = TColorAttr(kGryOnBlue()); // submenu disabled
            init = true;
        }
        static TPalette tp(pal, (ushort)32);
        return tp;
    }
};
class FoxStatusLine : public TStatusLine {
public:
    FoxStatusLine(const TRect& r, TStatusDef& d) : TStatusLine(r, d) {}
    TPalette& getPalette() const override {
        static TColorAttr pal[16];
        static bool init = false;
        if (!init) {
            for (int i=0;i<16;++i) pal[i] = TColorAttr(kYelOnRed());
            pal[0] = TColorAttr(kYelOnRed()); // normal text
            pal[1] = TColorAttr(kWhtOnRed()); // accelerator
            pal[2] = TColorAttr(kWhtOnRed()); // disabled-ish
            pal[3] = TColorAttr(kWhtOnRed()); // frames/other
            init = true;
        }
        static TPalette tp(pal, (ushort)16);
        return tp;
    }
};
#else
class FoxMenuBar : public TMenuBar { public: FoxMenuBar(const TRect& r, TMenu* m):TMenuBar(r,m){} };
class FoxStatusLine : public TStatusLine { public: FoxStatusLine(const TRect& r, TStatusDef& d):TStatusLine(r,d){} };
#endif

// ---------- App ----------
class TDotTalkApp : public AppBase {
public:
    TDotTalkApp();
    ~TDotTalkApp() override;

    void handleEvent(TEvent& event) override;
    void idle() override;

    static TMenuBar* initMenuBar(TRect r);
    static TStatusLine* initStatusLine(TRect r);

    void commandDialog(xbase::DbArea& area, const std::string& prefill = "");
    void setDbArea(xbase::DbArea* a) { dbArea_ = a; }

    void enqueueChunk(const std::string& s) {
        std::lock_guard<std::mutex> lk(qMutex_);
        pending_.push_back(s);
        if (!flushPosted_) {
            flushPosted_ = true;
            TEvent ev; ev.what = evCommand; ev.message.command = cmFlushLog; putEvent(ev);
        }
    }
    void flushPending() {
        std::vector<std::string> chunks;
        {
            std::lock_guard<std::mutex> lk(qMutex_);
            if (pending_.empty()) return;
            chunks.swap(pending_);
            flushPosted_ = false;
        }
        if (outView_) outView_->appendBatch(chunks);
    }
    void detachOutputWindow(TOutputWindow* win) {
        if (outWin_ == win) { outWin_ = nullptr; outView_ = nullptr; }
    }

private:
    void createBackground();
    void createOutputWindow();
    void createCommandBar();
    void showOutputWindow();
    void installRedirection();
    void restoreRedirection();
    void setOutputMode(OutputMode m);
    void openFindDialog();
    void doSelfTest(size_t n);
    void executeCommandLine(const std::string& line);
    void showHelp();
    void showAbout();
    void loadIni();
    void saveIni();
    void snapshotGeometry();

#ifdef _WIN32
    void startCStdCapture();
    void stopCStdCapture();
    static void readerThreadLoop(TDotTalkApp* app, int fd);
    int oldStdout_{-1}, oldStderr_{-1};
    int pipeOut_[2]{-1,-1}, pipeErr_[2]{-1,-1};
    std::thread tOut_, tErr_;
    std::atomic<bool> running_{false};
#endif

    std::vector<std::string> history_;
    int         histIndex_{-1};
    std::string lastInput_;

    std::string lastFind_;
    bool        findCaseInsensitive_{true};

    std::mutex qMutex_;
    std::vector<std::string> pending_;
    bool flushPosted_{false};

    xbase::DbArea* dbArea_ {nullptr};
    TOutputWindow* outWin_  {nullptr};
    TLogView*      outView_ {nullptr};

    TWindow*   cmdWin_{nullptr};
    TCmdInput* cmdInput_{nullptr};

    std::unique_ptr<CoutRedirectBuf> coutBuf_;
    std::unique_ptr<CoutRedirectBuf> cerrBuf_;
    std::streambuf* oldCout_ {nullptr};
    std::streambuf* oldCerr_ {nullptr};
    OutputMode mode_ {OutputMode::ToWindow};

    UiIni ini_;
};

// ---------- OutputWindow impl ----------
void TOutputWindow::close() {
    if (owner_) owner_->detachOutputWindow(this);
    TWindow::close();
}

// ---------- App impl ----------
TDotTalkApp::TDotTalkApp()
    : TProgInit(&TDotTalkApp::initStatusLine,
                &TDotTalkApp::initMenuBar,
                &TDotTalkApp::initDeskTop)
{
    createBackground();

    std::string base = UiIni::appdataPath();
#ifdef _WIN32
    ini_.path = base + "\\ui.ini";
#else
    ini_.path = base + "/ui.ini";
#endif
    loadIni();

    createOutputWindow();
    createCommandBar();

    static bool s_gui_registered = false;
    if (!s_gui_registered) {
        if (auto* eng = shell_engine()) register_shell_commands(*eng, /*include_ui_cmds=*/false);
        s_gui_registered = true;
    }

    installRedirection();
    setOutputMode(ini_.mode);
}

TDotTalkApp::~TDotTalkApp() {
    snapshotGeometry();
    saveIni();
    restoreRedirection();
}

void TDotTalkApp::createBackground() {
    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;
    TRect r(0, 0, S(sw), S(sh));
    auto* bg = new TBackground(r, 0x20); // TV default (space char)
    deskTop->insert(bg);
}

void TDotTalkApp::createOutputWindow() {
    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;

    int L = std::max(0, ini_.outX);
    int T = std::max(1, ini_.outY);
    int R = std::min(sw - 1, ini_.outX + ini_.outW);
    int B = std::min(sh - 1, ini_.outY + ini_.outH);
    if (R - L < 30) { L = 2; R = sw - 2; }
    if (B - T <  6) { T = 3; B = sh - 6; }
    TRect r(S(L), S(T), S(R), S(B));

    outWin_ = new TOutputWindow(r, "Output", 0, this);
    outWin_->flags   |= wfGrow | wfZoom;
    outWin_->growMode = gfGrowAll;

    const int winW = r.b.x - r.a.x;
    const int winH = r.b.y - r.a.y;

    const TRect hRect(S(1),        S(winH - 2),
                      S(winW - 2), S(winH - 1));
    const TRect vRect(S(winW - 2), S(1),
                      S(winW - 1), S(winH - 2));

    auto* h = new TScrollBar(hRect);
    auto* v = new TScrollBar(vRect);

    outWin_->insert(h); outWin_->insert(v);

    const TRect vr(S(1), S(1), S(winW - 2), S(winH - 2));
    outView_ = new TLogView(vr, h, v);
    outView_->setMaxLines(100000);
    outWin_->insert(outView_);

    deskTop->insert(outWin_);
}

void TDotTalkApp::createCommandBar() {
    const int sw = TScreen::screenWidth;
    const int sh = TScreen::screenHeight;

    const int width  = (ini_.cmdW > 0) ? ini_.cmdW : (sw - 2);
    const int left   = (ini_.cmdX > 0) ? ini_.cmdX : 1;
    const int height = (ini_.cmdH > 0) ? ini_.cmdH : 4;
    const int top    = (ini_.cmdY > 0) ? ini_.cmdY : (sh - height - 1);

    TRect r(S(left), S(top), S(left + width), S(top + height));
    cmdWin_ = new TWindow(r, "Command", 0);
    cmdWin_->flags |= wfGrow | wfZoom;
    cmdWin_->growMode = gfGrowHiX | gfGrowHiY;

    TRect ir(S(2), S(1), S(width - 14), S(2));
    cmdInput_ = new TCmdInput(ir, 256);
    if (!ini_.lastCmd.empty()) cmdInput_->prefill(ini_.lastCmd);
    cmdWin_->insert(cmdInput_);

    cmdWin_->insert(new TButton(TRect(S(width - 12), S(1), S(width - 2), S(3)), "~R~un", cmRunCmd, bfDefault));

    deskTop->insert(cmdWin_);
}

void TDotTalkApp::showOutputWindow() {
    if (!outWin_) createOutputWindow();
    if (outWin_) { outWin_->show(); outWin_->select(); }
}

void TDotTalkApp::installRedirection() {
    oldCout_ = std::cout.rdbuf();
    oldCerr_ = std::cerr.rdbuf();
    coutBuf_ = std::make_unique<CoutRedirectBuf>(this, oldCout_, mode_);
    cerrBuf_ = std::make_unique<CoutRedirectBuf>(this, oldCerr_, mode_);
    std::cout.rdbuf(coutBuf_.get());
    std::cerr.rdbuf(cerrBuf_.get());
    std::cout << "Output redirected to in-app 'Output' window.\n" << std::flush;
#ifdef _WIN32
    startCStdCapture();
#endif
}

void TDotTalkApp::restoreRedirection() {
#ifdef _WIN32
    stopCStdCapture();
#endif
    if (oldCout_) std::cout.rdbuf(oldCout_);
    if (oldCerr_) std::cerr.rdbuf(oldCerr_);
}

void TDotTalkApp::setOutputMode(OutputMode m) {
    mode_ = m;
    ini_.mode = m;
    if (coutBuf_) coutBuf_->setMode(m);
    if (cerrBuf_) cerrBuf_->setMode(m);
    const char* label = (m == OutputMode::ToWindow) ? "window" :
                        (m == OutputMode::ToConsole) ? "console" : "both";
    std::cout << "[Output mode: " << label << "]\n";
}

void TDotTalkApp::commandDialog(xbase::DbArea& area, const std::string& prefill) {
    while (true) {
        TDialog* d = new TDialog(TRect(10, 5, 70, 15), "Command Window");
        d->options |= ofCentered;

        auto* input = new TCmdInput(TRect(2, 2, 58, 3), 256);
        d->insert(input);
        if (!prefill.empty()) input->prefill(prefill);
        input->select();

        d->insert(new TButton(TRect(40, 4, 58, 6), "~O~K", cmOK, bfDefault));
        d->insert(new TButton(TRect(40, 6, 58, 8), "~C~ancel", cmCancel, bfNormal));

        const ushort result = deskTop->execView(d);
        if (result != cmOK) { destroy(d); break; }

        const char* raw = input->data;
        std::string trimmed = raw ? std::string(raw) : std::string();
        destroy(d);

        if (trimmed.empty()) continue;
        executeCommandLine(trim(trimmed));
    }
}

void TDotTalkApp::executeCommandLine(const std::string& line) {
    const std::string trimmed = trim(line);
    if (trimmed.empty() || !dbArea_) return;

    lastInput_ = trimmed;
    ini_.lastCmd = trimmed;
    if (history_.empty() || history_.back() != trimmed) history_.push_back(trimmed);
    histIndex_ = -1;

    std::cout << "> " << trimmed << "\n";

    std::istringstream tok(trimmed);
    std::string cmdToken; tok >> cmdToken;
    std::string U = textio::up(cmdToken);

    auto confirm = [&](const char*, const char* msg)->bool{
        ushort r = messageBox(msg, mfWarning | mfYesNoCancel);
        return r == cmYes;
    };

    if (U == "/SELFTEST" || U == "SELFTEST") {
        size_t n = 10000;
        if (tok.good()) tok >> n;
        doSelfTest(n);
        return;
    }
    if (U == "PACK") {
        if (!confirm("Confirm", "PACK will rebuild and may be slow. Continue?")) return;
    } else if (U == "ZAP") {
        if (!confirm("Confirm", "ZAP will delete ALL records. Continue?")) return;
    }

    if (!dli::registry().run(*dbArea_, U, tok)) {
        messageBox(("Unknown or failed command: " + cmdToken).c_str(), mfError | mfOKButton);
    }
}

void TDotTalkApp::openFindDialog() {
    TDialog* d = new TDialog(TRect(18, 6, 64, 12), "Find");
    d->options |= ofCentered;
    TInputLine* input = new TInputLine(TRect(2, 2, 40, 3), 128);
    d->insert(input);
    if (!lastFind_.empty()) input->setData((void*)lastFind_.c_str());
    input->select();

    const char* ciLbl = findCaseInsensitive_ ? "Case: ~I~nsensitive" : "Case: ~S~ensitive";
    const ushort cmToggleCase = 301;
    d->insert(new TButton(TRect(42, 2, 62, 4), ciLbl, cmToggleCase, bfNormal));
    d->insert(new TButton(TRect(2, 4, 12, 6), "~F~ind", cmOK, bfDefault));
    d->insert(new TButton(TRect(14, 4, 26, 6), "~C~ancel", cmCancel, bfNormal));

    while (true) {
        ushort res = deskTop->execView(d);
        if (res == cmToggleCase) {
            findCaseInsensitive_ = !findCaseInsensitive_;
            destroy(d);
            openFindDialog();
            return;
        } else if (res == cmOK || res == cmCancel) {
            if (res == cmOK) {
                const char* raw = input->data;
                std::string term = raw ? std::string(raw) : std::string();
                if (!term.empty()) {
                    lastFind_ = term;
                    const int startRow = outView_ ? outView_->delta.y + 1 : 0;
                    int found = outView_ ? outView_->findNext(term, startRow, findCaseInsensitive_) : -1;
                    if (found < 0) messageBox("Not found.", mfInformation | mfOKButton);
                }
            }
            destroy(d);
            return;
        }
    }
}

void TDotTalkApp::doSelfTest(size_t n) {
    std::cout << "[SELFTEST] Generating " << n << " lines..." << std::endl;
    for (size_t i = 1; i <= n; ++i) {
        std::cout << "cout line " << i << " lorem ipsum\n";
        if ((i % 50) == 0) std::cout.flush();
#ifdef _WIN32
        std::printf("printf line %zu dolor sit amet\n", i);
        if ((i % 50) == 0) std::fflush(stdout);
#else
        printf("printf line %zu dolor sit amet\n", i);
        if ((i % 50) == 0) fflush(stdout);
#endif
    }
    std::cout << "[SELFTEST] Done.\n";
}

void TDotTalkApp::idle() {
    AppBase::idle();
    std::cout.flush();
    std::cerr.flush();
    flushPending();
}

void TDotTalkApp::handleEvent(TEvent& event) {
    AppBase::handleEvent(event);

    if (event.what == evCommand && event.message.command == cmFlushLog) {
        flushPending();
        clearEvent(event);
        return;
    }

    if (event.what == evKeyDown) {
        if (event.keyDown.keyCode == kbF5) {
            if (dbArea_) commandDialog(*dbArea_, lastInput_);
            clearEvent(event); return;
        }
        if (event.keyDown.keyCode == kbF7) {
            if (outView_) outView_->clearAll();
            clearEvent(event); return;
        }
        if (event.keyDown.keyCode == kbCtrlF) {
            openFindDialog();
            clearEvent(event); return;
        }
        if (event.keyDown.keyCode == kbF3) {
            if (!lastFind_.empty() && outView_) {
                const int startRow = outView_->delta.y + 1;
                int found = outView_->findNext(lastFind_, startRow, findCaseInsensitive_);
                if (found < 0) messageBox("Not found.", mfInformation | mfOKButton);
            }
            clearEvent(event); return;
        }
        if (event.keyDown.keyCode == kbF2) {
            showOutputWindow();
            clearEvent(event); return;
        }
        if (event.keyDown.keyCode == kbF1) {
            showHelp();
            clearEvent(event); return;
        }
        if (event.keyDown.keyCode == kbCtrlQ) {
            if (cmdWin_) { cmdWin_->show(); cmdWin_->select(); if (cmdInput_) cmdInput_->select(); }
            clearEvent(event); return;
        }
    }

    if (event.what == evCommand) {
        auto prefill = [&](const char* s){
            if (dbArea_) commandDialog(*dbArea_, s);
        };

        switch (event.message.command) {
            case cmFoxProCommand: prefill(""); break;

            // File
            case cmUse:         prefill("USE ");     break;
            case cmCloseFile:   prefill("CLOSE");    break;
            case cmSelect:      prefill("SELECT ");  break;
            case cmArea:        prefill("AREA");     break;
            case cmFoxProExit: /* default quit */    break;

            // Edit
            case cmFind:        prefill("FIND ");    break;
            case cmReplace:     prefill("REPLACE "); break;

            // Database
            case cmBrowse:      prefill("BROWSE");   break;
            case cmDisplay:     prefill("DISPLAY");  break;
            case cmFields:      prefill("FIELDS");   break;
            case cmStruct:      prefill("STRUCT");   break;
            case cmLocate:      prefill("LOCATE ");  break;
            case cmPack:        prefill("PACK");     break;
            case cmZap:         prefill("ZAP");      break;

            // Index
            case cmSeek:        prefill("SEEK ");    break;
            case cmSetOrder:    prefill("SETORDER ");break;
#if DOTTALK_WITH_INDEX
            case cmAscend:      prefill("ASCEND");   break;
            case cmDescend:     prefill("DESCEND");  break;
#endif

            // Window / Output
            case cmShowOutput:  showOutputWindow();                    break;
            case cmModeWindow:  setOutputMode(OutputMode::ToWindow);   break;
            case cmModeConsole: setOutputMode(OutputMode::ToConsole);  break;
            case cmModeBoth:    setOutputMode(OutputMode::ToBoth);     break;

            // Command bar
            case cmRunCmd: {
                if (!cmdInput_) break;
                const char* raw = cmdInput_->data;
                std::string s = raw ? std::string(raw) : std::string();
                s = trim(s);
                if (!s.empty()) executeCommandLine(s);
                if (cmdInput_) { cmdInput_->selectAll(True); cmdInput_->focus(); }
                break;
            }
            case cmFocusCmd: {
                if (cmdWin_) { cmdWin_->show(); cmdWin_->select(); if (cmdInput_) cmdInput_->select(); }
                break;
            }

            default: break;
        }
        clearEvent(event);
        return;
    }
}

TMenuBar* TDotTalkApp::initMenuBar(TRect r) {
    r.b.y = (short)(r.a.y + 1);
#if DOTTALK_FOXPRO_PALETTE
    return new FoxMenuBar(
        r,
        new TMenu(
            *new TSubMenu("~F~ile", kbAltF) +
                *new TMenuItem("~U~se...",         cmUse,        kbNoKey) +
                *new TMenuItem("~C~lose",          cmCloseFile,  kbNoKey) +
                *new TMenuItem("~S~elect Area...", cmSelect,     kbNoKey) +
                *new TMenuItem("~A~rea",           cmArea,       kbNoKey) +
                newLine() +
                *new TMenuItem("E~x~it",           cmFoxProExit, kbAltX, hcNoContext, "Alt-X") +
            *new TSubMenu("~E~dit", kbAltE) +
                *new TMenuItem("~F~ind...",        cmFind,       kbCtrlF, hcNoContext, "Ctrl-F") +
                *new TMenuItem("~R~eplace...",     cmReplace,    kbNoKey) +
            *new TSubMenu("~D~atabase", kbAltD) +
                *new TMenuItem("~B~rowse",         cmBrowse,     kbNoKey) +
                *new TMenuItem("~D~isplay",        cmDisplay,    kbNoKey) +
                *new TMenuItem("~F~ields",         cmFields,     kbNoKey) +
                *new TMenuItem("St~r~uct",         cmStruct,     kbNoKey) +
                *new TMenuItem("~L~ocate...",      cmLocate,     kbNoKey) +
                *new TMenuItem("~P~ack",           cmPack,       kbNoKey) +
                *new TMenuItem("~Z~ap",            cmZap,        kbNoKey) +
            *new TSubMenu("~W~indow", kbAltW) +
                *new TMenuItem("~O~utput Window",  cmShowOutput, kbF2, hcNoContext, "F2") +
                *new TSubMenu("~O~utput Mode",     kbNoKey) +
                    *new TMenuItem("To ~W~indow",  cmModeWindow,  kbNoKey) +
                    *new TMenuItem("To ~C~onsole", cmModeConsole, kbNoKey) +
                    *new TMenuItem("~B~oth",       cmModeBoth,    kbNoKey) +
            *new TSubMenu("~H~elp", kbAltH) +
                *new TMenuItem("~C~ommand Window...", cmFoxProCommand, kbNoKey) +
                *new TMenuItem("~A~bout",             cmHelp,          kbNoKey)
        )
    );
#else
    return new TMenuBar(r,
        *new TSubMenu("~F~ile", kbAltF) +
            *new TMenuItem("~U~se...",         cmUse,        kbNoKey) +
            *new TMenuItem("~C~lose",          cmCloseFile,  kbNoKey) +
            *new TMenuItem("~S~elect Area...", cmSelect,     kbNoKey) +
            *new TMenuItem("~A~rea",           cmArea,       kbNoKey) +
            newLine() +
            *new TMenuItem("E~x~it",           cmFoxProExit, kbAltX, hcNoContext, "Alt-X") +
        *new TSubMenu("~E~dit", kbAltE) +
            *new TMenuItem("~F~ind...",        cmFind,       kbCtrlF, hcNoContext, "Ctrl-F") +
            *new TMenuItem("~R~eplace...",     cmReplace,    kbNoKey) +
        *new TSubMenu("~D~atabase", kbAltD) +
            *new TMenuItem("~B~rowse",         cmBrowse,     kbNoKey) +
            *new TMenuItem("~D~isplay",        cmDisplay,    kbNoKey) +
            *new TMenuItem("~F~ields",         cmFields,     kbNoKey) +
            *new TMenuItem("St~r~uct",         cmStruct,     kbNoKey) +
            *new TMenuItem("~L~ocate...",      cmLocate,     kbNoKey) +
            *new TMenuItem("~P~ack",           cmPack,       kbNoKey) +
            *new TMenuItem("~Z~ap",            cmZap,        kbNoKey) +
        *new TSubMenu("~W~indow", kbAltW) +
            *new TMenuItem("~O~utput Window",  cmShowOutput, kbF2, hcNoContext, "F2") +
            *new TSubMenu("~O~utput Mode",     kbNoKey) +
                *new TMenuItem("To ~W~indow",  cmModeWindow,  kbNoKey) +
                *new TMenuItem("To ~C~onsole", cmModeConsole, kbNoKey) +
                *new TMenuItem("~B~oth",       cmModeBoth,    kbNoKey) +
        *new TSubMenu("~H~elp", kbAltH) +
            *new TMenuItem("~C~ommand Window...", cmFoxProCommand, kbNoKey) +
            *new TMenuItem("~A~bout",             cmHelp,          kbNoKey)
    );
#endif
}

TStatusLine* TDotTalkApp::initStatusLine(TRect r) {
    r.a.y = (short)(r.b.y - 1);
#if DOTTALK_FOXPRO_PALETTE
    return new FoxStatusLine(
        r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~F1~ Help",       kbF1,   cmHelp) +
            *new TStatusItem("~F10~ Menu",      kbF10,  cmMenu) +
            *new TStatusItem("~Ctrl-Q~ Command",kbCtrlQ,cmFocusCmd) +
            *new TStatusItem("~F2~ Output",     kbF2,   cmShowOutput) +
            *new TStatusItem("~F5~ Repeat",     kbF5,   cmFoxProCommand) +
            *new TStatusItem("~F7~ Clear",      kbF7,   0) +
            *new TStatusItem("~Alt-X~ Exit",    kbAltX, cmFoxProExit)
    );
#else
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~F1~ Help",       kbF1,   cmHelp) +
            *new TStatusItem("~F10~ Menu",      kbF10,  cmMenu) +
            *new TStatusItem("~Ctrl-Q~ Command",kbCtrlQ,cmFocusCmd) +
            *new TStatusItem("~F2~ Output",     kbF2,   cmShowOutput) +
            *new TStatusItem("~F5~ Repeat",     kbF5,   cmFoxProCommand) +
            *new TStatusItem("~F7~ Clear",      kbF7,   0) +
            *new TStatusItem("~Alt-X~ Exit",    kbAltX, cmFoxProExit)
    );
#endif
}

void TDotTalkApp::showHelp() {
    messageBox(
        "FoxPro-style keys:\n"
        "  F10  Menu     Alt-X Exit\n"
        "  Ctrl-Q Focus Command Bar\n"
        "  F2   Output   F7   Clear Output\n"
        "  Ctrl-F Find   F3   Find Next\n"
        "\n"
        "Use bottom Command bar to type commands (Enter to run).\n"
        "Examples: USE customers, BROWSE, LOCATE LASTNAME='SMITH', PACK, ZAP\n",
        mfInformation | mfOKButton
    );
}

void TDotTalkApp::showAbout() {
    messageBox(
        "DotTalk++ FoxPro UI\n"
        "  TVision-based shell with FoxPro 2.6a style menus & command bar.\n",
        mfInformation | mfOKButton
    );
}

void TDotTalkApp::loadIni() { ini_.load(); }

void TDotTalkApp::snapshotGeometry() {
    if (outWin_) { ini_.outX = outWin_->origin.x; ini_.outY = outWin_->origin.y;
                   ini_.outW = outWin_->size.x;   ini_.outH = outWin_->size.y; }
    if (cmdWin_) { ini_.cmdX = cmdWin_->origin.x; ini_.cmdY = cmdWin_->origin.y;
                   ini_.cmdW = cmdWin_->size.x;   ini_.cmdH = cmdWin_->size.y; }
}

void TDotTalkApp::saveIni() { ini_.save(); }

#ifdef _WIN32
void TDotTalkApp::readerThreadLoop(TDotTalkApp* app, int fd) {
    char buf[64 * 1024];
    while (app->running_.load(std::memory_order_relaxed)) {
        int n = _read(fd, buf, (unsigned)sizeof(buf));
        if (n <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(3)); continue; }
        app->enqueueChunk(std::string(buf, buf + n));
    }
}
void TDotTalkApp::startCStdCapture() {
    running_.store(true, std::memory_order_relaxed);
    oldStdout_ = _dup(1);
    oldStderr_ = _dup(2);
    if (_pipe(pipeOut_, 64 * 1024, _O_TEXT) == 0) {
        _dup2(pipeOut_[1], 1);
        tOut_ = std::thread(&TDotTalkApp::readerThreadLoop, this, pipeOut_[0]);
    }
    if (_pipe(pipeErr_, 64 * 1024, _O_TEXT) == 0) {
        _dup2(pipeErr_[1], 2);
        tErr_ = std::thread(&TDotTalkApp::readerThreadLoop, this, pipeErr_[0]);
    }
}
void TDotTalkApp::stopCStdCapture() {
    running_.store(false, std::memory_order_relaxed);
    if (oldStdout_ != -1) { _dup2(oldStdout_, 1); _close(oldStdout_); oldStdout_ = -1; }
    if (oldStderr_ != -1) { _dup2(oldStderr_, 2); _close(oldStderr_); oldStderr_ = -1; }
    if (pipeOut_[1] != -1) { _close(pipeOut_[1]); pipeOut_[1] = -1; }
    if (pipeErr_[1] != -1) { _close(pipeErr_[1]); pipeErr_[1] = -1; }
    if (tOut_.joinable()) tOut_.join();
    if (tErr_.joinable()) tErr_.join();
    if (pipeOut_[0] != -1) { _close(pipeOut_[0]); pipeOut_[0] = -1; }
    if (pipeErr_[0] != -1) { _close(pipeErr_[0]); pipeErr_[0] = -1; }
}
#endif

// ---------- redirect impl ----------
void CoutRedirectBuf::flushBuffer() {
    if (pptr() == pbase()) return;
    std::string s(pbase(), pptr());
    pbump(-(int)(pptr() - pbase()));
    if (mode_ == OutputMode::ToWindow || mode_ == OutputMode::ToBoth) {
        if (app_) app_->enqueueChunk(s);
    }
    if ((mode_ == OutputMode::ToConsole || mode_ == OutputMode::ToBoth) && original_) {
        original_->sputn(s.data(), (std::streamsize)s.size());
        original_->pubsync();
    }
}

// ---------- Entry point ----------
void cmd_FOXPRO(xbase::DbArea& area, std::istringstream& /*args*/) {
#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE)
    TDotTalkApp app;
    app.setDbArea(&area);
    std::cout << "Launching DotTalk FoxPro UI...\n";
    app.run();
#else
    std::cout << "TVISION is not available in this build.\n";
#endif
}
