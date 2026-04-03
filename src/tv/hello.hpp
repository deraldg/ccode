#if defined(DOTTALK_WITH_TV) || defined(DOTTALK_TV_AVAILABLE)
#ifndef HELLO_HPP
#define HELLO_HPP

#define Uses_TApplication
#define Uses_TEvent
#define Uses_TRect
#define Uses_TMenuBar
#define Uses_TStatusLine
#include <tvision/tv.h>

const int GreetThemCmd = 100;

class THelloApp : public TApplication
{
public:
    THelloApp();
    virtual void handleEvent(TEvent& event);
    static TMenuBar *initMenuBar(TRect r);
    static TStatusLine *initStatusLine(TRect r);
private:
    void greetingBox();
};

#endif // HELLO_HPP
#else
// TVision not enabled: header declarations not required in non-TV build.

#endif  // TVISION guard
