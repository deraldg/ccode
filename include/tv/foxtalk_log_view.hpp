#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

// ---- TVision uses ----
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDrawBuffer
#define Uses_TKeys

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

class FoxtalkLogView : public TScroller {
public:
    FoxtalkLogView(const TRect& bounds, TScrollBar* hScroll, TScrollBar* vScroll);

    void setMaxLines(std::size_t n);
    void appendBatch(const std::vector<std::string>& chunks);
    void clearAll();

    void draw() override;
    void handleEvent(TEvent& ev) override;

private:
    void appendInternal(const std::string& s);
    void finalizeAppend();

private:
    std::deque<std::string> lines_;
    std::size_t maxLines_{100000};
    short maxWidth_{0};
};

} // namespace foxtalk