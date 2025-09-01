#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace browse {
struct Column { std::string name; int width{10}; };
struct Cell   { std::string text; };
struct Row    { std::vector<Cell> cells; std::uint32_t recno{0}; };
struct Viewport { int firstRow{0}; int visibleRows{0}; int firstCol{0}; int visibleCols{0}; };
struct BrowseViewModel {
    std::vector<Column> columns;
    std::vector<Row>    rows;
    Viewport            vp;
    int                 selected{0};
    int                 totalRows{0};
    std::string         status;
    bool                windowed{true};
    int                 leftPadding{1};
};
} // namespace browse
