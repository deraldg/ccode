#include "tv/foxtalk_layout.hpp"
#include "tv/foxtalk_util.hpp"

#include <cstdlib>
#include <fstream>
#include <string>

namespace foxtalk {

std::filesystem::path iniPath()
{
#ifdef _WIN32
    const char* app = std::getenv("APPDATA");
    std::filesystem::path base = app ? app : ".";
    base /= "Foxtalk";
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : ".";
    base /= ".foxtalk";
#endif

    std::filesystem::create_directories(base);
    return base / "foxtalk.ini";
}

bool loadLayoutState(LayoutState& state, int maxHistory)
{
    std::ifstream in(iniPath());
    if (!in)
        return false;

    state = LayoutState{};
    std::string line;

    while (std::getline(in, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        const std::string k = trim(line.substr(0, pos));
        const std::string v = trim(line.substr(pos + 1));

        try {
            if (k == "lastInput")
                state.lastInput = v;
            else if (k.rfind("hist.", 0) == 0)
                state.history.push_back(v);

            else if (k == "out.x")
                state.output.x = std::stoi(v);
            else if (k == "out.y")
                state.output.y = std::stoi(v);
            else if (k == "out.w")
                state.output.w = std::stoi(v);
            else if (k == "out.h")
                state.output.h = std::stoi(v);
            else if (k == "out.zoom")
                state.output.zoomed = (v == "1");

            else if (k == "cmd.x")
                state.command.x = std::stoi(v);
            else if (k == "cmd.y")
                state.command.y = std::stoi(v);
            else if (k == "cmd.w")
                state.command.w = std::stoi(v);
            else if (k == "cmd.h")
                state.command.h = std::stoi(v);
            else if (k == "cmd.zoom")
                state.command.zoomed = (v == "1");
        }
        catch (...) {
            // Ignore malformed values and continue.
        }
    }

    if (static_cast<int>(state.history.size()) > maxHistory) {
        state.history.erase(
            state.history.begin(),
            state.history.begin() + (static_cast<int>(state.history.size()) - maxHistory)
        );
    }

    return true;
}

bool saveLayoutState(const LayoutState& state)
{
    std::ofstream out(iniPath(), std::ios::trunc);
    if (!out)
        return false;

    out << "lastInput=" << state.lastInput << "\n";

    for (std::size_t i = 0; i < state.history.size(); ++i)
        out << "hist." << i << "=" << state.history[i] << "\n";

    out << "out.x="    << state.output.x      << "\n";
    out << "out.y="    << state.output.y      << "\n";
    out << "out.w="    << state.output.w      << "\n";
    out << "out.h="    << state.output.h      << "\n";
    out << "out.zoom=" << (state.output.zoomed ? 1 : 0) << "\n";

    out << "cmd.x="    << state.command.x      << "\n";
    out << "cmd.y="    << state.command.y      << "\n";
    out << "cmd.w="    << state.command.w      << "\n";
    out << "cmd.h="    << state.command.h      << "\n";
    out << "cmd.zoom=" << (state.command.zoomed ? 1 : 0) << "\n";

    return true;
}

void clampGeometryToScreen(WindowGeometry& g, int screenW, int screenH,
                           int defaultX, int defaultY, int defaultW, int defaultH,
                           int minW, int minH)
{
    if (g.w <= 0 || g.h <= 0) {
        g.x = defaultX;
        g.y = defaultY;
        g.w = defaultW;
        g.h = defaultH;
        return;
    }

    g.w = clampInt(g.w, minW, screenW - 1);
    g.h = clampInt(g.h, minH, screenH - 1);

    g.x = clampInt(g.x, 0, std::max(0, screenW - g.w));
    g.y = clampInt(g.y, 0, std::max(0, screenH - g.h));
}

} // namespace foxtalk