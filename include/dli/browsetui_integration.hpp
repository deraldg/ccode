#pragma once
// dli/browsetui_integration.hpp
// High-level helper that threads dli::screen_* fast painting into a browse TUI loop.
// No 'cli' symbols; namespace dli only.

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <cstdint>

namespace dli {

// Represents one visible row in the grid
struct ViewRow {
    int64_t recno = 0;
    std::vector<std::string> cells; // one preformatted string per column
};

// Data provider fetches rows by absolute row index in the dataset or by recno.
// Return false if row not available.
using FetchRowByIndex = std::function<bool(int absoluteIndex, ViewRow& out)>;
using FetchRowByRecno = std::function<bool(int64_t recno, ViewRow& out)>;

// Integration object that owns Fast Paint usage for a grid view.
class BrowsePaint {
public:
    // cellXs: horizontal positions (0-based columns) for each column start.
    // width,height: logical render area.
    // vt_capable: pass result of screen_enable_vt(true) here.
    BrowsePaint(std::vector<int> cellXs, int width, int height, bool vt_capable);

    // Set status line Y (defaults to bottom line: height-1)
    void set_status_y(int y);

    // Replace entire visible window with rows starting at absoluteIndex 'topIndex'.
    // Uses 'fetch' to obtain ViewRow for each visible line.
    void load_window(int topIndex, int visibleCount, const FetchRowByIndex& fetch);

    // Redraw the whole visible window (fast; uses diffs).
    void redraw_all();

    // Move highlight bar from oldIdx (visible) to newIdx (visible), reusing cached line strings.
    void move_highlight(int oldVisible, int newVisible);

    // Patch a single cell for the given visible row and column.
    // Also updates our cached copy.
    void patch_cell(int visibleRow, int col, std::string_view newText, bool highlighted);

    // Update the status line with a single write.
    void set_status(std::string_view s);

    // Scroll by delta lines; reuses cached rows and fetches only newly visible ones.
    void scroll(int delta, const FetchRowByIndex& fetch);

    // Returns current top absolute index in dataset.
    int top_index() const { return m_topIndex; }

    // Returns true if VT highlight is used.
    bool vt() const { return m_vt; }

    // Cached visible rows
    const std::vector<ViewRow>& rows() const { return m_rows; }

    // Column X positions
    const std::vector<int>& cell_xs() const { return m_cellXs; }

private:
    void render_row_line(int visY, const ViewRow& row, int highlightCol);

private:
    std::vector<int> m_cellXs;
    int mW=0, mH=0;
    int mStatusY=0;
    bool m_vt=false;

    int m_topIndex=0;            // absolute index of first visible row
    int m_highlightVisible=-1;   // which visible row is highlighted (-1 = none)
    std::vector<ViewRow> m_rows; // cached visible rows (size == visibleCount)
};

} // namespace dli
