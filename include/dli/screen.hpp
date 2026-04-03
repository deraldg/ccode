#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace dli {

// Core screen management
void screen_init(int width, int height);
void screen_shutdown();
bool screen_enable_vt(bool enable);
void screen_clear(bool clear_console = true);

void screen_write_line(int y, std::string_view text);
void screen_write_span(int x, int y, std::string_view text);
void screen_set_cursor(int x, int y, bool visible);

int screen_width();
int screen_height();
const std::vector<std::string>& screen_shadow();

// === VT100 / ANSI ===
std::string vt_inverse(std::string_view s, bool vt);
std::string vt_reset(bool vt = true);

// === 16-color ===
std::string vt_fg(std::string_view text, int fg_code, bool bold = false, bool vt = true);

// === 256-color ===
std::string vt_256_fg(unsigned int id, bool vt = true);
std::string vt_256_bg(unsigned int id, bool vt = true);
std::string vt_fg_256(std::string_view text, unsigned int fg_id, bool bold = false, bool vt = true);
std::string vt_fg_bg_256(std::string_view text,
                         unsigned int fg_id, unsigned int bg_id,
                         bool bold = false, bool vt = true);

// === Truecolor RGB (24-bit) ===
std::string vt_rgb_fg(uint8_t r, uint8_t g, uint8_t b, bool vt = true);
std::string vt_rgb_bg(uint8_t r, uint8_t g, uint8_t b, bool vt = true);

std::string vt_fg_rgb(std::string_view text,
                      uint8_t r, uint8_t g, uint8_t b,
                      bool bold = false, bool vt = true);

std::string vt_fg_bg_rgb(std::string_view text,
                         uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
                         uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                         bool bold = false, bool vt = true);

} // namespace dli