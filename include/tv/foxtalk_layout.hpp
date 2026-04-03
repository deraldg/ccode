#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace foxtalk {

struct WindowGeometry {
    int x{-1};
    int y{-1};
    int w{-1};
    int h{-1};
    bool zoomed{false};
};

struct LayoutState {
    std::string lastInput;
    std::vector<std::string> history;
    WindowGeometry output;
    WindowGeometry command;
};

std::filesystem::path iniPath();

bool loadLayoutState(LayoutState& state, int maxHistory);
bool saveLayoutState(const LayoutState& state);

void clampGeometryToScreen(WindowGeometry& g, int screenW, int screenH,
                           int defaultX, int defaultY, int defaultW, int defaultH,
                           int minW, int minH);

} // namespace foxtalk