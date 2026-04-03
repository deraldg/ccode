#pragma once

#include <string>

// ---- TVision uses ----
#define Uses_TView
#define Uses_TRect
#define Uses_TDrawBuffer

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

class TMsgLine : public TView {
public:
    explicit TMsgLine(const TRect& bounds);

    void set(const std::string& s);
    void clear();

    void draw() override;

private:
    std::string text_;
};

} // namespace foxtalk