#pragma once
#include <cstdlib>
#include <string>
namespace browse {
inline bool env_flag(const char* name, bool def=false) {
    if (const char* v = std::getenv(name)) {
        std::string s(v);
        for (auto& c: s) c = (char)tolower((unsigned char)c);
        return s=="1" || s=="true" || s=="on" || s=="yes";
    }
    return def;
}
inline bool use_v2()              { return env_flag("BROWSE_TUI_V2", false); }
inline bool allow_write()         { return env_flag("BROWSE_TUI_ALLOW_WRITE", false); }
inline bool renderer_diff_mode()  { return env_flag("BROWSE_TUI_DIFF", true); }
inline bool windowed_default()    { return env_flag("BROWSE_TUI_WINDOWED", true); }
} // namespace browse
