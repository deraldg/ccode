#pragma once
#include "browse/browse_view_model.hpp"

namespace browse {

struct Rect { int x{0}, y{0}, w{0}, h{0}; };

struct Layout {
    Rect frame;
    Rect window;
    Rect header;
    Rect list;
    Rect status;
};

struct LayoutSettings {
    bool windowed{true};
    int  minInnerMargin{1};
    int  desiredWidth{0};   // 0 = auto
    int  desiredHeight{0};  // 0 = auto
};

class LayoutManager {
public:
    Layout compute(int termW, int termH, const LayoutSettings& s) const;
};

} // namespace browse
