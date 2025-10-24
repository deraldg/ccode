#pragma once
// dli/screen.hpp — tiny dirty-region console renderer (Windows-optimized)
// No 'cli' symbols; all under namespace dli.
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace dli {

// Initialize renderer and allocate shadow buffer (width x height);
// hides cursor for smoother draws. Call shutdown() before exit.
void screen_init(int width, int height);

// Restore cursor and clean up.
void screen_shutdown();

// Optionally enable/disable VT mode (ANSI escape processing).
// On Windows this attempts to turn on ENABLE_VIRTUAL_TERMINAL_PROCESSING.
// Returns true if VT mode is active.
bool screen_enable_vt(bool enable);

// Clear internal shadow to blanks and optionally clear the console.
void screen_clear(bool clear_console=true);

// Stage write of a single line by diffing against the shadow and writing
// only changed spans. 'text' is padded/truncated to current width.
void screen_write_line(int y, std::string_view text);

// Patch a small rectangular span (actually a single line span) at (x,y).
void screen_write_span(int x, int y, std::string_view text);

// Update the cursor position and show/hide as desired.
void screen_set_cursor(int x, int y, bool visible);

// Return current logical width/height used by the renderer.
int screen_width();
int screen_height();

// Shadow buffer accessor (width-sized strings of current on-screen state).
const std::vector<std::string>& screen_shadow();

// Utility: build a highlighted (inverse) string without ANSI if not available.
// If vt is true, wraps with ESC[7m/ESC[27m. Otherwise, just returns the text;
// the caller should draw highlight by redrawing normal + inverted attributes
// using platform API if desired.
std::string vt_inverse(std::string_view s, bool vt);

} // namespace dli
