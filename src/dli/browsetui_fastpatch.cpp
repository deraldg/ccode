// dli/browsetui_fastpatch.cpp ? surgical helpers to speed up browsetui
#include "dli/screen.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace dli {

// Render a whole row from preformatted cells with optional highlight on one column.
// cellXs: x positions for each column (same length as cells).
// If highlightCol >= 0, that cell is drawn inverted using VT if available.
void browsetui_render_row(int rowY,
                          const std::vector<int>& cellXs,
                          const std::vector<std::string>& cells,
                          int highlightCol /*=-1*/,
                          bool vt_capable /*=true*/) {
    const int n = (int)cells.size();
    for (int c = 0; c < n; ++c){
        const int x = (c < (int)cellXs.size()) ? cellXs[c] : 0;
        const std::string& txt = cells[c];
        if (c == highlightCol && !txt.empty()){
            screen_write_span(x, rowY, vt_inverse(txt, vt_capable));
        } else {
            screen_write_span(x, rowY, txt);
        }
    }
}

// Patch just one cell (fast path for in-cell edits).
void browsetui_patch_cell(int rowY, int cellX, std::string_view text, bool highlighted, bool vt_capable){
    if (highlighted) {
        std::string inv = vt_inverse(text, vt_capable);
        screen_write_span(cellX, rowY, inv);
    } else {
        screen_write_span(cellX, rowY, text);
    }
}

// Update the status line (single write).
void browsetui_set_status(int y, std::string_view text){
    screen_write_line(y, text);
}

// Move highlight bar from one row to another by redrawing only those two lines.
void browsetui_move_highlight(int oldY, int newY,
                              const std::string& oldLine,
                              const std::string& newLine){
    if (oldY >= 0) screen_write_line(oldY, oldLine);
    if (newY >= 0) screen_write_line(newY, newLine);
}

} // namespace dli



