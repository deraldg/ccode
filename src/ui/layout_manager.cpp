#include "ui/layout_manager.hpp"
#include <algorithm> // std::min/std::max

namespace browse {

Layout LayoutManager::compute(int termW, int termH, const LayoutSettings& s) const {
    Layout L;
    L.frame  = {0, 0, termW, termH};
    L.header = {0, 0, termW, 1};
    L.status = {0, termH > 0 ? termH - 1 : 0, termW, 1};

    int listY = 1;
    int listH = termH - 2; // minus header/status
    if (listH < 0) listH = 0;

    if (!s.windowed) {
        L.window = L.frame;
        L.list   = {0, listY, termW, listH};
        return L;
    }

    int margin = s.minInnerMargin < 1 ? 1 : s.minInnerMargin;

    int winW = s.desiredWidth  > 0 ? std::min(s.desiredWidth,  termW - 2*margin) : termW - 2*margin;
    int winH = s.desiredHeight > 0 ? std::min(s.desiredHeight, termH - 2*margin) : termH - 2*margin;

    if (winW < 20) winW = std::max(0, termW - 2*margin);
    if (winH < 5)  winH = std::max(0, termH - 2*margin);

    int winX = (termW - winW) / 2;
    int winY = (termH - winH) / 2;

    L.window = {winX, winY, winW, winH};
    L.header = {winX, winY, winW, 1};
    L.status = {winX, winY + winH - 1, winW, 1};
    L.list   = {winX, winY + 1, winW, winH - 2};
    return L;
}

} // namespace browse
