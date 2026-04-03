#include "dli/browsetui_integration.hpp"
#include "dli/screen.hpp"
#include <algorithm>

namespace dli {

BrowsePaint::BrowsePaint(std::vector<int> cellXs, int width, int height, bool vt_capable)
    : m_cellXs(std::move(cellXs))
    , mW(width)
    , mH(height)
    , m_vt(vt_capable)
    , mStatusY(std::max(0, height - 1))
{
}

void BrowsePaint::set_status_y(int y) {
    mStatusY = std::max(0, y);
}

void BrowsePaint::load_window(int topIndex, int visibleCount, const FetchRowByIndex& fetch) {
    m_topIndex = topIndex;
    m_rows.assign(visibleCount, ViewRow{});
    m_highlightVisible = -1;

    for (int i = 0; i < visibleCount; ++i) {
        if (fetch(topIndex + i, m_rows[i])) {
            render_row_line(i, m_rows[i], false);
        }
    }
}

void BrowsePaint::redraw_all() {
    for (int i = 0; i < (int)m_rows.size(); ++i) {
        render_row_line(i, m_rows[i], (i == m_highlightVisible));
    }
}

void BrowsePaint::move_highlight(int oldVisible, int newVisible) {
    if (oldVisible >= 0 && oldVisible < (int)m_rows.size()) {
        render_row_line(oldVisible, m_rows[oldVisible], false);
    }
    m_highlightVisible = newVisible;
    if (newVisible >= 0 && newVisible < (int)m_rows.size()) {
        render_row_line(newVisible, m_rows[newVisible], true);
    }
}

void BrowsePaint::patch_cell(int visibleRow, int col, std::string_view newText, bool highlighted) {
    if (visibleRow < 0 || visibleRow >= (int)m_rows.size()) return;
    if (col < 0 || col >= (int)m_rows[visibleRow].cells.size()) return;

    m_rows[visibleRow].cells[col] = std::string(newText);

    int x = (col < (int)m_cellXs.size()) ? m_cellXs[col] : 0;
    if (highlighted) {
        std::string inv = vt_inverse(newText, m_vt);
        screen_write_span(x, visibleRow, inv);
    } else {
        screen_write_span(x, visibleRow, newText);
    }
}

void BrowsePaint::set_status(std::string_view s) {
    screen_write_line(mStatusY, s);
}

void BrowsePaint::scroll(int delta, const FetchRowByIndex& fetch) {
    if (delta == 0 || m_rows.empty()) return;

    m_topIndex += delta;
    if (m_topIndex < 0) m_topIndex = 0;

    std::vector<ViewRow> newRows(m_rows.size());
    int n = (int)m_rows.size();

    if (delta > 0) {
        for (int i = 0; i < n - delta; ++i) newRows[i] = m_rows[i + delta];
        for (int i = n - delta; i < n; ++i) fetch(m_topIndex + i, newRows[i]);
    } else {
        int k = -delta;
        for (int i = n - 1; i >= k; --i) newRows[i] = m_rows[i - k];
        for (int i = 0; i < k; ++i) fetch(m_topIndex + i, newRows[i]);
    }

    m_rows.swap(newRows);
    redraw_all();
}

// === FIXED: matches the header exactly ===
void BrowsePaint::render_row_line(int visY, const ViewRow& row, bool highlightWholeRow) {
    for (int c = 0; c < (int)row.cells.size(); ++c) {
        int x = (c < (int)m_cellXs.size()) ? m_cellXs[c] : 0;
        const std::string& t = row.cells[c];

        if (highlightWholeRow) {
            // Nice truecolor highlight: white text on deep blue background
            std::string colored = vt_fg_bg_rgb(t, 255, 255, 255, 0, 50, 120, true, m_vt);
            screen_write_span(x, visY, colored);
        } else {
            screen_write_span(x, visY, t);
        }
    }
}

} // namespace dli