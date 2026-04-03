#pragma once

#include <string>

// ---- TVision uses ----
#define Uses_TRect
#define Uses_TEvent
#define Uses_TInputLine
#define Uses_TProgram

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

class TCmdInput : public TInputLine {
public:
    TCmdInput(const TRect& bounds, ushort maxLen);

    void prefill(const std::string& s);
    void handleEvent(TEvent& ev) override;

private:
    void clearAll();
    void killToStart();
    void killToEnd();
};

} // namespace foxtalk