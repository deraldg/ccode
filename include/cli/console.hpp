#pragma once
#include <string>

struct Size { int cols; int rows; };

struct Console {
    virtual ~Console() = default;
    virtual void clear() = 0;
    virtual void move_to(int x, int y) = 0;
    virtual void draw_text(int x, int y, const std::string& s, int clip_to = -1) = 0;
    virtual void draw_frame(int left, int top, int width, int height) = 0;
    virtual int  get_key() = 0;   // blocking key read
    virtual Size size() = 0;
};

Console* make_console(); // factory: picks Win or Posix
