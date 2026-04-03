#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <cstdint>

namespace dli {

struct ViewRow {
    int64_t recno = 0;
    std::vector<std::string> cells;
};

using FetchRowByIndex = std::function<bool(int absoluteIndex, ViewRow& out)>;

class BrowsePaint {
public:
    BrowsePaint(std::vector<int> cellXs, int width, int height, bool vt_capable);

    void set_status_y(int y);
    void load_window(int topIndex, int visibleCount, const FetchRowByIndex& fetch);
    void redraw_all();
    void move_highlight(int oldVisible, int newVisible);
    void patch_cell(int visibleRow, int col, std::string_view newText, bool highlighted);
    void set_status(std::string_view s);
    void scroll(int delta, const FetchRowByIndex& fetch);

    int top_index() const { return m_topIndex; }
    bool vt() const { return m_vt; }
    const std::vector<ViewRow>& rows() const { return m_rows; }
    const std::vector<int>& cell_xs() const { return m_cellXs; }

private:
    // highlightWholeRow = true → draw row with highlight colors (truecolor)
    void render_row_line(int visY, const ViewRow& row, bool highlightWholeRow);

private:
    std::vector<int> m_cellXs;
    int mW = 0, mH = 0;
    int mStatusY = 0;
    bool m_vt = false;

    int m_topIndex = 0;
    int m_highlightVisible = -1;
    std::vector<ViewRow> m_rows;
};

} // namespace dli