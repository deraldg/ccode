// ---- TVision uses ----
#define Uses_TRect
#define Uses_TWindow
#define Uses_TButton

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

#include "tv/foxtalk_command_window.hpp"
#include "tv/foxtalk_cmd_input.hpp"
#include "tv/foxtalk_ids.hpp"
#include "tv/foxtalk_message_line.hpp"
#include "tv/foxtalk_util.hpp"

namespace foxtalk {

FoxtalkCommandWindow::FoxtalkCommandWindow(const TRect& bounds, const char* title, ushort number)
    : TWindow(bounds, title, number)
    , TWindowInit(&TWindow::initFrame)
{
    flags |= (wfMove | wfGrow | wfZoom);
    growMode = gfGrowHiX | gfGrowHiY;

    const int ww = static_cast<int>(bounds.b.x - bounds.a.x);

    TRect ir(S(2), S(1), S(ww - 14), S(2));
    input_ = new TCmdInput(ir, 1024);
    insert(input_);

    insert(new TButton(
        TRect(S(ww - 12), S(1), S(ww - 2), S(3)),
        "~R~un",
        cmRunCmd,
        bfDefault
    ));

    msgLine_ = new TMsgLine(TRect(S(2), S(2), S(ww - 2), S(3)));
    insert(msgLine_);
}

TCmdInput* FoxtalkCommandWindow::input() const
{
    return input_;
}

TMsgLine* FoxtalkCommandWindow::messageLine() const
{
    return msgLine_;
}

std::string FoxtalkCommandWindow::getInputText() const
{
    if (!input_)
        return {};

    char buf[2048]{};
    input_->getData(buf);
    return std::string(buf);
}

void FoxtalkCommandWindow::setInputText(const std::string& s)
{
    if (!input_)
        return;

    input_->setData((void*)s.c_str());
}

void FoxtalkCommandWindow::focusInput()
{
    if (!input_)
        return;

    select();
    input_->select();
    input_->focus();
}

void FoxtalkCommandWindow::setMessage(const std::string& s)
{
    if (msgLine_)
        msgLine_->set(s);
}

void FoxtalkCommandWindow::clearMessage()
{
    if (msgLine_)
        msgLine_->clear();
}

} // namespace foxtalk