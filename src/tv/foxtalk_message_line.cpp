#include "tv/foxtalk_message_line.hpp"
#include "tv/foxtalk_util.hpp"

namespace foxtalk {

TMsgLine::TMsgLine(const TRect& bounds)
    : TView(bounds)
{
    growMode = gfGrowHiX;
    options &= ~ofFramed;
}

void TMsgLine::set(const std::string& s)
{
    text_ = s;
    drawView();
}

void TMsgLine::clear()
{
    text_.clear();
    drawView();
}

void TMsgLine::draw()
{
    TDrawBuffer b;
    const ushort color = getColor(0x0303);

    std::string line = text_;
    const int sx = static_cast<int>(size.x);

    if (static_cast<int>(line.size()) < sx)
        line.append(static_cast<std::size_t>(sx - static_cast<int>(line.size())), ' ');
    else if (static_cast<int>(line.size()) > sx)
        line.resize(static_cast<std::size_t>(sx));

    b.moveStr(0, line.c_str(), color);
    writeLine(S(0), S(0), size.x, S(1), b);
}

} // namespace foxtalk