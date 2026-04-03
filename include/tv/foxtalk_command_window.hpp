#pragma once

#include <string>

// ---- TVision uses ----
#define Uses_TWindow
#define Uses_TRect

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

class TCmdInput;
class TMsgLine;

class FoxtalkCommandWindow : public TWindow {
public:
    FoxtalkCommandWindow(const TRect& bounds, const char* title, ushort number = 0);

    TCmdInput* input() const;
    TMsgLine* messageLine() const;

    std::string getInputText() const;
    void setInputText(const std::string& s);
    void focusInput();
    void setMessage(const std::string& s);
    void clearMessage();

private:
    TCmdInput* input_{nullptr};
    TMsgLine* msgLine_{nullptr};
};

} // namespace foxtalk