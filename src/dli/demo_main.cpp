// dli/demo_main.cpp ? optional minimal demo (build only if DLI_DEMO_MAIN defined)
// Shows windowed vs full-screen toggle with F6. No DBF access?just fake fields.
#ifdef DLI_DEMO_MAIN

#include "dli/screen.hpp"
#include "dli/browsetui_window.hpp"
#include "dli/view_mode.hpp"
#include "dli/set_view.hpp"
#include <vector>
#include <string>
#include <cstdio>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace demo {

static void draw_window(){
    int W = dli::screen_width();
    int H = dli::screen_height();

    std::vector<dli::FormField> fields = {
        {"LAST_NAME",  "Grimwood"},
        {"FIRST_NAME", "Derald"},
        {"AGE",        "68"},
        {"MEMO",       "This is a demo memo line."}
    };

    dli::FormOptions opt;
    opt.inner_left_pad  = 1;
    opt.inner_right_pad = 1;
    opt.use_box_chars   = false;

    dli::form_render_windowed(fields, W, H, opt);
    dli::screen_set_cursor(0, H-1, false);
    dli::screen_write_line(H-1, dli::view_status_label());
}

static void draw_fullscreen(){
    int W = dli::screen_width();
    int H = dli::screen_height();
    // Fake grid: two rows, three columns
    dli::screen_write_line(0,  "LAST_NAME     FIRST_NAME    AGE");
    dli::screen_write_line(1,  "Grimwood      Derald        68 ");
    dli::screen_write_line(2,  "Doe           Jane          42 ");
    for (int y=3; y<H-1; ++y) dli::screen_write_line(y, "");
    dli::screen_write_line(H-1, dli::view_status_label());
}

static void repaint(){
    if (dli::get_view_mode() == dli::ViewMode::Window) draw_window();
    else draw_fullscreen();
}

} // namespace demo

int main(){
    // Init fast paint area (fixed size for demo)
    dli::screen_init(80, 24);
    dli::screen_clear(true);
    dli::screen_enable_vt(true);
    dli::set_view_mode(dli::ViewMode::Window);
    demo::repaint();

#ifdef _WIN32
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
    for(;;){
        INPUT_RECORD rec;
        DWORD n=0;
        if (!ReadConsoleInput(hIn, &rec, 1, &n)) break;
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown){
            auto vk = rec.Event.KeyEvent.wVirtualKeyCode;
            std::string status;
            if (dli::handle_toggle_view_hotkey((int)vk, &status)) {
                dli::screen_write_line(23, status);
                demo::repaint();
            } else if (vk == VK_ESCAPE) {
                break;
            }
        }
    }
#endif

    dli::screen_shutdown();
    return 0;
}

#endif // DLI_DEMO_MAIN



