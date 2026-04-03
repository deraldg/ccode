#include "tv/foxtalk_workspace_view.hpp"
#include "tv/foxtalk_util.hpp"

#include <string>
#include <vector>

namespace foxtalk {

FoxtalkWorkspaceView::FoxtalkWorkspaceView(const TRect& bounds)
    : TView(bounds)
{
    options |= ofSelectable;
    growMode = gfGrowHiX | gfGrowHiY;
}

void FoxtalkWorkspaceView::setSnapshot(const WorkspaceSnapshot& snapshot)
{
    snapshot_ = snapshot;
    drawView();
}

void FoxtalkWorkspaceView::clearSnapshot()
{
    snapshot_ = WorkspaceSnapshot{};
    drawView();
}

std::vector<std::string> FoxtalkWorkspaceView::buildLines() const
{
    std::vector<std::string> lines;

    lines.push_back("Workspace");
    lines.push_back("---------");

    if (!snapshot_.hasOpenTable) {
        lines.push_back("Area   : " + std::to_string(snapshot_.areaNumber));
        lines.push_back("Alias  : (none)");
        lines.push_back("File   : (no file open)");
        lines.push_back("Recno  : ");
        lines.push_back("Count  : ");
        lines.push_back("Deleted: ");
        lines.push_back("Order  : ");
        return lines;
    }

    lines.push_back("Area   : " + std::to_string(snapshot_.areaNumber));
    lines.push_back("Alias  : " + snapshot_.alias);
    lines.push_back("File   : " + snapshot_.fileName);
    lines.push_back("Recno  : " + std::to_string(snapshot_.recno));
    lines.push_back("Count  : " + std::to_string(snapshot_.recCount));
    lines.push_back(std::string("Deleted: ") + (snapshot_.deleted ? "YES" : "NO"));
    lines.push_back("Order  : " + snapshot_.orderName);

    if (!snapshot_.filterText.empty())
        lines.push_back("Filter : " + snapshot_.filterText);

    return lines;
}

void FoxtalkWorkspaceView::draw()
{
    TDrawBuffer b;
    const ushort color = getColor(0x0301);
    const auto lines = buildLines();

    for (short row = 0; row < size.y; ++row) {
        std::string text =
            (row >= 0 && row < static_cast<short>(lines.size()))
                ? lines[static_cast<std::size_t>(row)]
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

} // namespace foxtalk
