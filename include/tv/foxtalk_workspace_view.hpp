#pragma once

#include <string>
#include <vector>

// ---- TVision uses ----
#define Uses_TView
#define Uses_TRect
#define Uses_TDrawBuffer

#if DOTTALK_TV_AVAILABLE
#include <tvision/tv.h>
#endif

namespace foxtalk {

struct WorkspaceSnapshot {
    int areaNumber{-1};
    std::string alias;
    std::string fileName;
    long long recno{0};
    long long recCount{0};
    bool deleted{false};
    std::string orderName;
    std::string filterText;
    bool hasOpenTable{false};
};

class FoxtalkWorkspaceView : public TView {
public:
    explicit FoxtalkWorkspaceView(const TRect& bounds);

    void setSnapshot(const WorkspaceSnapshot& snapshot);
    void clearSnapshot();

    void draw() override;

private:
    std::vector<std::string> buildLines() const;

private:
    WorkspaceSnapshot snapshot_;
};

} // namespace foxtalk
