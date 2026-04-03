#include "tv/foxtalk_log_view.hpp"
#include "tv/foxtalk_util.hpp"

#include <algorithm>

namespace foxtalk {

FoxtalkLogView::FoxtalkLogView(const TRect& bounds, TScrollBar* hScroll, TScrollBar* vScroll)
    : TScroller(bounds, hScroll, vScroll)
{
    options |= ofFramed | ofSelectable;
    growMode = gfGrowHiX | gfGrowHiY;
    setLimit(S(1), S(1));
}

void FoxtalkLogView::setMaxLines(std::size_t n)
{
    maxLines_ = std::max<std::size_t>(4096, n);
}

void FoxtalkLogView::appendBatch(const std::vector<std::string>& chunks)
{
    for (const auto& s : chunks)
        appendInternal(s);

    finalizeAppend();
}

void FoxtalkLogView::clearAll()
{
    lines_.clear();
    maxWidth_ = 0;
    setLimit(S(1), S(1));
    scrollTo(S(0), S(0));
    drawView();
}

void FoxtalkLogView::draw()
{
    TDrawBuffer b;
    const ushort color = getColor(0x0301);

    for (short row = 0; row < size.y; ++row) {
        const int idx = delta.y + row;

        std::string text =
            (idx >= 0 && idx < static_cast<int>(lines_.size()))
                ? lines_[static_cast<std::size_t>(idx)]
                : std::string();

        const int sx = static_cast<int>(size.x);

        if (static_cast<int>(text.size()) < sx)
            text.append(static_cast<std::size_t>(sx - static_cast<int>(text.size())), ' ');
        else if (static_cast<int>(text.size()) > sx)
            text.resize(static_cast<std::size_t>(sx));

        b.moveStr(0, text.c_str(), color);
        writeLine(S(0), row, size.x, S(1), b);
    }
}

void FoxtalkLogView::handleEvent(TEvent& ev)
{
    TScroller::handleEvent(ev);

    if (ev.what == evKeyDown) {
        switch (ev.keyDown.keyCode) {
        case kbPgUp:
            scrollTo(delta.x, S(std::max<int>(0, delta.y - (size.y > 1 ? size.y - 1 : 1))));
            clearEvent(ev);
            break;

        case kbPgDn:
            scrollTo(delta.x, S(std::min<int>(limit.y - size.y, delta.y + (size.y > 1 ? size.y - 1 : 1))));
            clearEvent(ev);
            break;

        case kbHome:
            scrollTo(delta.x, S(0));
            clearEvent(ev);
            break;

        case kbEnd:
            scrollTo(delta.x, S(std::max<int>(0, limit.y - size.y)));
            clearEvent(ev);
            break;
        }
    }
}

void FoxtalkLogView::appendInternal(const std::string& s)
{
    std::size_t start = 0;

    while (start <= s.size()) {
        const std::size_t nl = s.find('\n', start);
        const std::string line =
            (nl == std::string::npos)
                ? s.substr(start)
                : s.substr(start, nl - start);

        if (!line.empty() || nl != std::string::npos) {
            lines_.push_back(line);

            while (lines_.size() > maxLines_)
                lines_.pop_front();

            const short w = static_cast<short>(line.size());
            if (w > maxWidth_)
                maxWidth_ = w;
        }

        if (nl == std::string::npos)
            break;

        start = nl + 1;
    }
}

void FoxtalkLogView::finalizeAppend()
{
    const short limX = std::max<short>(S(1), maxWidth_);
    const short limY = std::max<short>(S(1), static_cast<short>(lines_.size()));

    setLimit(limX, limY);
    scrollTo(S(0), S(std::max<int>(0, static_cast<int>(lines_.size()) - size.y)));
    drawView();
}

} // namespace foxtalk