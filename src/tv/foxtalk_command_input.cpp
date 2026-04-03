// ---- TVision uses ----
#define Uses_TRect
#define Uses_TEvent
#define Uses_TInputLine
#define Uses_TProgram
#define Uses_TKeys

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

#include "tv/foxtalk_cmd_input.hpp"
#include "tv/foxtalk_ids.hpp"

namespace foxtalk {

TCmdInput::TCmdInput(const TRect& bounds, ushort maxLen)
    : TInputLine(bounds, maxLen)
{
}

void TCmdInput::prefill(const std::string& s)
{
    setData((void*)s.c_str());
    selectAll(True);
}

void TCmdInput::handleEvent(TEvent& ev)
{
    TInputLine::handleEvent(ev);

    if (ev.what == evKeyDown) {
        if (ev.keyDown.keyCode == kbEnter) {
            message(TProgram::application, evCommand, cmRunCmd, this);
            clearEvent(ev);
            return;
        }

        if (ev.keyDown.keyCode == kbEsc) {
            clearAll();
            clearEvent(ev);
            return;
        }

        if (ev.keyDown.keyCode == kbCtrlU) {
            killToStart();
            clearEvent(ev);
            return;
        }

        if (ev.keyDown.keyCode == kbCtrlK) {
            killToEnd();
            clearEvent(ev);
            return;
        }
    }
}

void TCmdInput::clearAll()
{
    static const char* empty = "";
    setData((void*)empty);
    selectAll(True);
    focus();
}

void TCmdInput::killToStart()
{
    if (!data)
        return;

    std::string s = data;

    if (curPos > 0 && curPos <= static_cast<int>(s.size()))
        s = s.substr(static_cast<std::size_t>(curPos));

    setData((void*)s.c_str());
    selectAll(False);
    focus();
}

void TCmdInput::killToEnd()
{
    if (!data)
        return;

    std::string s = data;

    if (curPos >= 0 && curPos <= static_cast<int>(s.size()))
        s.resize(static_cast<std::size_t>(curPos));

    setData((void*)s.c_str());
    selectAll(False);
    focus();
}

} // namespace foxtalk