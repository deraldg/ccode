#pragma once
#include <string>
#include <vector>

// Minimal structure describing a field to render in BROWSE
struct FieldView {
    std::string name;
    std::string value;
};

// Build the inner content lines for the BROWSE pane.
// - inner_width:  printable columns inside the frame (>= 0)
// - inner_height: printable rows    inside the frame (>= 0)
// - recno:        current record number to show on the first line
// - fields:       name/value pairs for the current record
// Returns exactly inner_height strings, each clipped to inner_width.
// Caller is responsible for placing these lines at (frame.left+1, frame.top+1)
// and for drawing the border itself.
std::vector<std::string>
build_browse_lines(int inner_width, int inner_height,
                   int recno,
                   const std::vector<FieldView>& fields);



