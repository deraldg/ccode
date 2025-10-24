#include "dli/screen.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace dli {

static int gW = 0, gH = 0;
static bool gVT = false;
static bool gCursorVisible = true;
static
#ifdef _WIN32
HANDLE
#else
int
#endif
hOut = 0;

static std::vector<std::string> gShadow;

#ifdef _WIN32
static void write_at(int x, int y, const char* data, size_t n) {
  DWORD dummy;
  COORD pos{(SHORT)x,(SHORT)y};
  SetConsoleCursorPosition(hOut, pos);
  WriteConsoleA(hOut, data, (DWORD)n, &dummy, nullptr);
}
#endif

int screen_width(){ return gW; }
int screen_height(){ return gH; }
const std::vector<std::string>& screen_shadow(){ return gShadow; }

bool screen_enable_vt(bool enable){
#ifdef _WIN32
  DWORD mode=0;
  if (!GetConsoleMode(hOut, &mode)) return gVT;
  if (enable) mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  else        mode &= ~ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (SetConsoleMode(hOut, mode)) gVT = enable;
  return gVT;
#else
  (void)enable;
  gVT = true;
  return gVT;
#endif
}

void screen_init(int width, int height){
  gW = std::max(1, width);
  gH = std::max(1, height);
  gShadow.assign(gH, std::string(gW, ' '));
#ifdef _WIN32
  hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  // Hide cursor by default for smooth painting
  CONSOLE_CURSOR_INFO ci{};
  ci.dwSize = 25;
  ci.bVisible = FALSE;
  SetConsoleCursorInfo(hOut, &ci);
  // Try to enable VT for nicer highlighting if available
  screen_enable_vt(true);
#endif
}

void screen_shutdown(){
#ifdef _WIN32
  // Restore cursor visibility
  CONSOLE_CURSOR_INFO ci{};
  ci.dwSize = 25;
  ci.bVisible = TRUE;
  SetConsoleCursorInfo(hOut, &ci);
#endif
  gShadow.clear();
  gW = gH = 0;
}

void screen_clear(bool clear_console){
  for (auto& line : gShadow) std::fill(line.begin(), line.end(), ' ');
#ifdef _WIN32
  if (clear_console){
    // Efficient full clear
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    GetConsoleScreenBufferInfo(hOut, &csbi);
    DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written = 0;
    COORD home{0,0};
    FillConsoleOutputCharacterA(hOut, ' ', cells, home, &written);
    FillConsoleOutputAttribute(hOut, csbi.wAttributes, cells, home, &written);
    SetConsoleCursorPosition(hOut, home);
  }
#else
  (void)clear_console;
#endif
}

static inline void pad_to_width(std::string& s){
  if ((int)s.size() < gW) s.resize(gW, ' ');
  else if ((int)s.size() > gW) s.resize(gW);
}

void screen_write_line(int y, std::string_view text){
  if (y < 0 || y >= gH) return;
  std::string line(text);
  pad_to_width(line);
#ifdef _WIN32
  // Diff against shadow and write only changed spans
  int i = 0, n = (int)line.size();
  while (i < n){
    while (i < n && line[i] == gShadow[y][i]) ++i;
    if (i >= n) break;
    int start = i;
    while (i < n && line[i] != gShadow[y][i]) ++i;
    int end = i;
    write_at(start, y, line.data()+start, end-start);
    std::copy(line.begin()+start, line.begin()+end, gShadow[y].begin()+start);
  }
#else
  // Fallback: print whole line once (non-Windows); caller can redirect to ncurses later
  (void)line;
#endif
}

void screen_write_span(int x, int y, std::string_view text){
  if (y < 0 || y >= gH) return;
  if (x < 0 || x >= gW) return;
  int maxlen = gW - x;
  int n = (int)text.size();
  if (n > maxlen) n = maxlen;
#ifdef _WIN32
  write_at(x, y, text.data(), n);
  std::copy(text.begin(), text.begin()+n, gShadow[y].begin()+x);
#else
  (void)n;
#endif
}

void screen_set_cursor(int x, int y, bool visible){
#ifdef _WIN32
  CONSOLE_CURSOR_INFO ci{};
  ci.dwSize = 25;
  ci.bVisible = visible ? TRUE : FALSE;
  SetConsoleCursorInfo(hOut, &ci);
  COORD pos{(SHORT)std::max(0,x),(SHORT)std::max(0,y)};
  SetConsoleCursorPosition(hOut, pos);
#else
  (void)x; (void)y; (void)visible;
#endif
}

std::string vt_inverse(std::string_view s, bool vt){
  if (!vt) return std::string(s);
  std::string out;
  out.reserve(s.size()+8);
  out += "\x1b[7m";
  out.append(s.begin(), s.end());
  out += "\x1b[27m";
  return out;
}

} // namespace dli
