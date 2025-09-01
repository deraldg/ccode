// src/cli/cmd_browsetui.cpp
//
// Cross-platform BROWSETUI with header/footer bars, frame,
// navigation, modals (LIST/DISPLAY), and type-aware editing with staging.
//
// - Windowed mode shows a smaller, centered form (~75% of terminal).
//   Press F6 to toggle Fullscreen <-> Windowed.
// - Edits are STAGED (not written immediately)
// - Ctrl+S saves all staged changes via transactional REPLACE_MULTI
// - On exit / record change, prompts to save if there are staged changes
//
// Assumes (from xbase.hpp):
//   FieldDef: .name, .type, .length, .decimals
//   DbArea  : fields(), get(1-based), recno(), recCount(), readCurrent(), isOpen()
//
// External commands provided elsewhere:
//   cmd_LIST, cmd_DISPLAY, cmd_GOTO, cmd_TOP, cmd_BOTTOM, cmd_CREATE, cmd_APPEND, cmd_DELETE
//
// Screen builder provided elsewhere:
//   build_browse_lines(int inner_w, int inner_h, int recno, const std::vector<FieldView>&)
//   (implemented in browse_lines.cpp)

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>

#include "xbase.hpp"
#include "cli/console.hpp"
#include "cli/browsetui.hpp"     // FieldView, Console, Size, etc.
#include "cli/memo_display.hpp"
#include "cli/replace_multi.hpp"

// externs for other command handlers
extern void cmd_LIST    (xbase::DbArea&, std::istringstream&);
extern void cmd_DISPLAY (xbase::DbArea&, std::istringstream&);
extern void cmd_GOTO    (xbase::DbArea&, std::istringstream&);
extern void cmd_TOP     (xbase::DbArea&, std::istringstream&);
extern void cmd_BOTTOM  (xbase::DbArea&, std::istringstream&);
extern void cmd_CREATE  (xbase::DbArea&, std::istringstream&);
extern void cmd_APPEND  (xbase::DbArea&, std::istringstream&);
extern void cmd_DELETE  (xbase::DbArea&, std::istringstream&);

// built in browse_lines.cpp (do NOT reimplement here)
extern std::vector<std::string>
build_browse_lines(int inner_w, int inner_h, int recno, const std::vector<FieldView>& fields);

// optional: index/order hooks
void order_notify_mutation(xbase::DbArea&) noexcept;

// -----------------------------
// small helpers
// -----------------------------
static inline void rtrim_inplace(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.pop_back();
}
static inline int get_current_recno_safe(xbase::DbArea& area) {
    return static_cast<int>(area.recno());
}

// Snapshot current record values (index-aligned to fields())
static std::vector<std::string> snapshot_record(xbase::DbArea& area) {
    std::vector<std::string> snap;
    const auto& F = area.fields();
    snap.reserve(F.size());
    (void)area.readCurrent();
    for (size_t i = 0; i < F.size(); ++i) {
        std::string v = area.get(static_cast<int>(i) + 1);
        rtrim_inplace(v);
        snap.push_back(std::move(v));
    }
    return snap;
}

// -----------------------------
// Key decoding (Windows + POSIX)
// -----------------------------
enum class Key {
    Other, Esc, Quit, Enter, Up, Down, Home, End, G, L, D, E, Help, CtrlS, PageUp, PageDown, F1, F2, F3, F4, F5, F6
};

static Key read_key(Console& con) {
    int c = con.get_key();
    if (c == 27) { // ESC or CSI sequence
        int c2 = con.get_key();
        if (c2 == '[') {
            int c3 = con.get_key();
            switch (c3) {
                case 'A': return Key::Up;
                case 'B': return Key::Down;
                case 'H': return Key::Home;
                case 'F': return Key::End;
                case '5': con.get_key(); return Key::PageUp;
                case '6': con.get_key(); return Key::PageDown;
                case 'P': con.get_key(); con.get_key(); return Key::F1;
                case 'Q': con.get_key(); con.get_key(); return Key::F2;
                case 'R': con.get_key(); con.get_key(); return Key::F3;
                case 'S': con.get_key(); con.get_key(); return Key::F4;
                default:  return Key::Other;
            }
        }
        if (c2 == 'O') {
            int c3 = con.get_key();
            if (c3 == 'P') return Key::F1;
            if (c3 == 'Q') return Key::F2;
            if (c3 == 'R') return Key::F3;
            if (c3 == 'S') return Key::F4;
        }
        return Key::Esc; // plain ESC
    }
#ifdef _WIN32
    if (c == 224 || c == 0) {
        int c2 = con.get_key();
        switch (c2) {
            case 72: return Key::Up;
            case 80: return Key::Down;
            case 71: return Key::Home;
            case 79: return Key::End;
            case 73: return Key::PageUp;
            case 81: return Key::PageDown;
            case 59: return Key::F1;
            case 60: return Key::F2;
            case 61: return Key::F3;
            case 62: return Key::F4;
            case 63: return Key::F5;
            case 64: return Key::F6;
            default: return Key::Other;
        }
    }
#endif
    if (c == '\r' || c == '\n') return Key::Enter;
    if (c == 19) return Key::CtrlS; // Ctrl+S
    if (c == 'q' || c == 'Q') return Key::Quit;
    if (c == 'g' || c == 'G') return Key::G;
    if (c == 'l' || c == 'L') return Key::L;
    if (c == 'd' || c == 'D') return Key::D;
    if (c == 'e' || c == 'E') return Key::E;
    if (c == '?' || c == 'h' || c == 'H') return Key::Help;
    return Key::Other;
}

// -----------------------------
// UI helpers: header/footer bars
// -----------------------------
static const char* SGR_RESET      = "\x1b[0m";
static const char* SGR_HEADER_BAR = "\x1b[1;37;42m"; // bold white on green
static const char* SGR_FOOTER_BAR = "\x1b[30;43m";   // black on amber
static const char* SGR_MENU_BAR   = "\x1b[47;30m";

static void draw_color_bar(Console& con, int y, int width,
                           const std::string& left, const std::string& right,
                           const char* style_sgr)
{
    std::string line;
    line.reserve(static_cast<size_t>(width));
    const int rlen = static_cast<int>(right.size());
    int space_for_left = std::max(0, width - rlen);

    std::string L = left;
    if (static_cast<int>(L.size()) > space_for_left) L.resize(static_cast<size_t>(space_for_left));
    line += L;
    if (space_for_left > static_cast<int>(L.size()))
        line.append(static_cast<size_t>(space_for_left - static_cast<int>(L.size())), ' ');

    std::string R = right;
    if (static_cast<int>(R.size()) > width) R.resize(static_cast<size_t>(width));
    line += R;

    if (static_cast<int>(line.size()) < width)
        line.append(static_cast<size_t>(width - static_cast<int>(line.size())), ' ');

    con.draw_text(0, y, std::string(style_sgr) + line + SGR_RESET);
}
static void draw_header_menu(Console& con, int y, int width) {
    std::string menu = " F1:Create | F2:Read | F3:Update | F4:Delete | F5:Append ";
    std::string line;
    line.reserve(static_cast<size_t>(width));
    int padding = std::max(0, width - static_cast<int>(menu.size()));
    int left_pad = padding / 2;
    int right_pad = padding - left_pad;
    line.append(static_cast<size_t>(left_pad), ' ');
    line += menu;
    line.append(static_cast<size_t>(right_pad), ' ');
    con.draw_text(0, y, std::string(SGR_MENU_BAR) + line + SGR_RESET);
}

// -----------------------------
// Footer prompts
// -----------------------------
static bool prompt_yes_no(Console& con, int footer_y, int term_cols, const std::string& question, bool& out_yes) {
    std::string s = question + " (Y/N, Esc=cancel)";
    if (static_cast<int>(s.size()) < term_cols) s.append(static_cast<size_t>(term_cols - static_cast<int>(s.size())), ' ');
    con.draw_text(0, footer_y, std::string(SGR_FOOTER_BAR) + s + SGR_RESET);
    std::cout.flush();
    for (;;) {
        int ch = con.get_key();
#ifdef _WIN32
        if (ch == 224 || ch == 0) { (void)con.get_key(); continue; }
#endif
        if (ch == 27) return false;
        if (ch == 'y' || ch == 'Y') { out_yes = true;  return true; }
        if (ch == 'n' || ch == 'N') { out_yes = false; return true; }
    }
}
static bool prompt_goto(Console& con, int footer_y, int term_cols, int& out_recno) {
    std::string buf;
    auto paint = [&](const std::string& pre){
        std::string s = pre + buf;
        if (static_cast<int>(s.size()) < term_cols) s.append(static_cast<size_t>(term_cols - static_cast<int>(s.size())), ' ');
        con.draw_text(0, footer_y, std::string(SGR_FOOTER_BAR) + s + SGR_RESET);
        std::cout.flush();
    };
    paint("Goto rec: ");
    for (;;) {
        int ch = con.get_key();
#ifdef _WIN32
        if (ch == 224 || ch == 0) { (void)con.get_key(); continue; }
#endif
        if (ch == 27) return false;
        if (ch == '\r' || ch == '\n') break;
        if (ch == 8 || ch == 127) { if (!buf.empty()) { buf.pop_back(); paint("Goto rec: "); } continue; }
        if (std::isdigit(ch)) { buf.push_back(static_cast<char>(ch)); paint("Goto rec: "); continue; }
    }
    if (buf.empty()) return false;
    out_recno = std::max(1, std::stoi(buf));
    return true;
}
static bool prompt_for_create_string(Console& con, int footer_y, int term_cols, const std::string& question, std::string& out) {
    std::string buf;
    auto paint = [&](const std::string& pre){
        std::string s = pre + buf;
        if (static_cast<int>(s.size()) < term_cols) s.append(static_cast<size_t>(term_cols - static_cast<int>(s.size())), ' ');
        con.draw_text(0, footer_y, std::string(SGR_FOOTER_BAR) + s + SGR_RESET);
        std::cout.flush();
    };
    paint(question);
    for (;;) {
        int ch = con.get_key();
        if (ch == 27) return false;
        if (ch == '\r' || ch == '\n') break;
        if (ch == 8 || ch == 127) { if (!buf.empty()) { buf.pop_back(); paint(question); } continue; }
        if (ch >= 32 && ch <= 126) { buf.push_back(static_cast<char>(ch)); paint(question); continue; }
    }
    out = buf;
    return true;
}

// -----------------------------
// Type-aware editing helpers
// -----------------------------
static std::string escape_double_quotes(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) if (s[i] == '"') s.insert(i++, "\\");
    return s;
}
static std::string normalize_date_YYYYMMDD(const std::string& in_raw) {
    std::string digits;
    digits.reserve(in_raw.size());
    for (char c : in_raw) if (std::isdigit(static_cast<unsigned char>(c))) digits.push_back(c);

    auto ok_md = [](int m, int d){ return m >= 1 && m <= 12 && d >= 1 && d <= 31; };

    if (digits.size() == 8) {
        // try YYYYMMDD
        int y = std::atoi(digits.substr(0,4).c_str());
        int m = std::atoi(digits.substr(4,2).c_str());
        int d = std::atoi(digits.substr(6,2).c_str());
        if (y > 0 && ok_md(m,d)) return digits;

        // try MMDDYYYY
        m = std::atoi(digits.substr(0,2).c_str());
        d = std::atoi(digits.substr(2,2).c_str());
        y = std::atoi(digits.substr(4,4).c_str());
        if (y > 0 && ok_md(m,d)) {
            char buf[9];
            std::snprintf(buf, sizeof(buf), "%04d%02d%02d", y, m, d);
            return std::string(buf, 8);
        }
    }
    return ""; // invalid
}
static std::string normalize_numeric(const std::string& in, int length, int decimals) {
    std::string s; s.reserve(in.size());
    bool seen_dot = false;

    for (char c : in) {
        if ((c == '-' && s.empty()) || std::isdigit(static_cast<unsigned char>(c))) {
            s.push_back(c);
            continue;
        }
        if (decimals > 0 && c == '.' && !seen_dot) {
            seen_dot = true; s.push_back(c); continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        return ""; // invalid char
    }
    if (s.empty() || s == "-" || s == ".") return "";

    if (decimals == 0) {
        size_t dot = s.find('.');
        if (dot != std::string::npos) s.erase(dot);
    } else {
        size_t dot = s.find('.');
        if (dot != std::string::npos) {
            size_t frac = s.size() - dot - 1;
            if (static_cast<int>(frac) > decimals) s.erase(dot + 1 + static_cast<size_t>(decimals));
        }
    }
    if (static_cast<int>(s.size()) > length) return "";
    return s;
}
static std::string normalize_logical_from_key(int ch, const std::string& currentTF) {
    auto toTF = [](char c)->std::string{
        switch (std::toupper(static_cast<unsigned char>(c))) {
            case 'T': case 'Y': case '1': return "T";
            case 'F': case 'N': case '0': return "F";
            default: return "";
        }
    };
    if (ch == ' ') return (currentTF=="T" ? "F" : "T");
    if (ch == '\r' || ch == '\n') return "";
    return toTF(static_cast<char>(ch));
}
static std::string make_replace_payload(char type, const std::string& valueNormalized) {
    switch (type) {
        case 'N': case 'n': return valueNormalized; // unquoted
        case 'L': case 'l': return valueNormalized; // T/F unquoted
        case 'D': case 'd':
        case 'C': case 'c':
        case 'M': case 'm':
        default:
            return std::string("\"") + escape_double_quotes(valueNormalized) + "\"";
    }
}

// Prompt for a value; return normalized text (type-aware)
static bool prompt_value_for_field(Console& con, int footer_y, int term_cols,
                                   xbase::DbArea& area,
                                   int fieldIndex, const std::string& fieldName,
                                   char fieldType, int fieldLen, int fieldDec,
                                   std::string& outNormalized)
{
    (void)area.readCurrent();
    std::string current = area.get(fieldIndex + 1);
    rtrim_inplace(current);
    const char typeCh = static_cast<char>(std::toupper(static_cast<unsigned char>(fieldType)));
    std::string base = "Set " + fieldName + " (" + std::string(1, typeCh) + "): ";

    if (typeCh == 'L') {
        std::string curTF = (!current.empty() && (current[0]=='T' || current[0]=='t')) ? "T" : "F";
        std::string msg   = base + "[ " + curTF + " ]  <Space toggles, Enter accepts, Esc cancels>";
        std::string s = msg; if (static_cast<int>(s.size()) < term_cols) s.append(static_cast<size_t>(term_cols - static_cast<int>(s.size())), ' ');
        con.draw_text(0, footer_y, std::string(SGR_FOOTER_BAR) + s + SGR_RESET);
        std::cout.flush();

        std::string nextTF = curTF;
        for (;;) {
            int ch = con.get_key();
#ifdef _WIN32
            if (ch == 224 || ch == 0) { (void)con.get_key(); continue; }
#endif
            if (ch == 27) return false;
            if (ch == '\r' || ch == '\n') { outNormalized = nextTF; return true; }
            std::string set = normalize_logical_from_key(ch, nextTF);
            if (!set.empty()) {
                nextTF = set;
                std::string s2 = base + "[ " + nextTF + " ]  <Space toggles, Enter accepts, Esc cancels>";
                if (static_cast<int>(s2.size()) < term_cols) s2.append(static_cast<size_t>(term_cols - static_cast<int>(s2.size())), ' ');
                con.draw_text(0, footer_y, std::string(SGR_FOOTER_BAR) + s2 + SGR_RESET);
                std::cout.flush();
            }
        }
    } else {
        std::string buf;
        auto paint = [&](){
            std::string example;
            switch (typeCh) {
                case 'D': example = " (YYYYMMDD, YYYY-MM-DD, or MM/DD/YYYY)"; break;
                case 'N': example = (fieldDec>0? " (number " + std::to_string(fieldLen) + "/" + std::to_string(fieldDec) + ")"
                                               : " (integer, max " + std::to_string(fieldLen) + ")"); break;
                default: break;
            }
            std::string s = base + "[" + current + "] : " + buf + example;
            if (static_cast<int>(s.size()) < term_cols) s.append(static_cast<size_t>(term_cols - static_cast<int>(s.size())), ' ');
            con.draw_text(0, footer_y, std::string(SGR_FOOTER_BAR) + s + SGR_RESET);
            std::cout.flush();
        };
        paint();
        for (;;) {
            int ch = con.get_key();
#ifdef _WIN32
            if (ch == 224 || ch == 0) { (void)con.get_key(); continue; }
#endif
            if (ch == 27) return false;
            if (ch == '\r' || ch == '\n') {
                std::string entered = buf.empty() ? current : buf;
                if (typeCh == 'D') {
                    std::string norm = normalize_date_YYYYMMDD(entered);
                    if (norm.empty()) { std::cout << '\a'; continue; }
                    outNormalized = norm;
                } else if (typeCh == 'N') {
                    std::string norm = normalize_numeric(entered, fieldLen, fieldDec);
                    if (norm.empty()) { std::cout << '\a'; continue; }
                    outNormalized = norm;
                } else {
                    outNormalized = entered; // C/M
                }
                return true;
            }
            if (ch == 8 || ch == 127) { if (!buf.empty()) buf.pop_back(); paint(); continue; }
            if (ch >= 32 && ch <= 126) { buf.push_back(static_cast<char>(ch)); paint(); continue; }
        }
    }
}

// -----------------------------
// Staging + editor UI
// -----------------------------
struct PendingEdits {
    std::unordered_map<std::string, std::string> by_name; // field name -> normalized
    void clear() { by_name.clear(); }
    bool empty() const { return by_name.empty(); }
    size_t count() const { return by_name.size(); }
};

static std::vector<FieldView> collect_field_views_with_staged(xbase::DbArea& area, const PendingEdits& st) {
    std::vector<FieldView> out;
    const auto& F = area.fields();
    out.reserve(F.size());
    (void)area.readCurrent();
    for (size_t i = 0; i < F.size(); ++i) {
        const std::string& name = F[i].name;
        std::string value;
        auto it = st.by_name.find(name);
        if (it != st.by_name.end()) value = it->second;
        else { value = area.get(static_cast<int>(i) + 1); rtrim_inplace(value); }
        out.push_back(FieldView{ name, value });
    }
    return out;
}

// Commit staged changes in one transaction-like call
static bool save_staged_transactional(Console& con, xbase::DbArea& area, const PendingEdits& staged) {
    if (staged.empty()) return true;
    std::vector<FieldUpdate> updates;
    updates.reserve(staged.by_name.size());
    for (const auto& kv : staged.by_name)
        updates.push_back(FieldUpdate{ kv.first, kv.second });

    std::string err;
    const bool ok = cmd_REPLACE_MULTI(area, updates, &err);
    if (!ok) {
        const int y = con.size().rows - 1;
        con.draw_text(0, y, "Error: " + err + " (press any key)", -1);
        (void)con.get_key();
    }
    return ok;
}

// Simple list editor (one field per line)
struct EditorState { int selected = 0; int scroll = 0; };

static void render_editor(Console& con,
                          const Size& term,
                          int frame_l, int frame_t, int frame_w, int frame_h,
                          xbase::DbArea& area,
                          const EditorState& st,
                          const PendingEdits& staged,
                          const std::string& status)
{
    const int inner_w = frame_w - 2;
    const int inner_h = frame_h - 2;

    con.clear();
    draw_color_bar(con, 0, term.cols, "EDIT RECORD",
                   "ESC: Back  \xE2\x86\x91/\xE2\x86\x93: Move  Enter: Edit  Ctrl+S: Save  Home/End: Jump",
                   SGR_HEADER_BAR);
    con.draw_frame(frame_l, frame_t, frame_w, frame_h);

    (void)area.readCurrent();
    const auto& F = area.fields();
    int total = static_cast<int>(F.size());
    int y = frame_t + 1;
    int visible = inner_h;
    int start = std::min(st.scroll, std::max(0, total - visible));
    int end = std::min(total, start + visible);

    for (int i = start; i < end; ++i, ++y) {
        const std::string& name = F[static_cast<size_t>(i)].name;
        std::string val;
        auto it = staged.by_name.find(name);
        if (it != staged.by_name.end()) val = it->second;
        else { val = area.get(i + 1); rtrim_inplace(val); }
        std::string line = name + ": " + val;
        if (static_cast<int>(line.size()) > inner_w) line.resize(static_cast<size_t>(inner_w));
        if (i == st.selected) con.draw_text(frame_l + 1, y, std::string("\x1b[7m") + line + SGR_RESET, inner_w);
        else                  con.draw_text(frame_l + 1, y, line, inner_w);
    }

    const int footer_y = term.rows - 1;
    std::string foot = status;
    if (!staged.empty()) foot += "  [staged: " + std::to_string(staged.count()) + "]";
    if (static_cast<int>(foot.size()) < term.cols) foot.append(static_cast<size_t>(term.cols - static_cast<int>(foot.size())), ' ');
    con.draw_text(0, footer_y, std::string(SGR_FOOTER_BAR) + foot + SGR_RESET);
    std::cout.flush();
}

static void run_editor(Console& con, xbase::DbArea& area, PendingEdits& staged) {
    std::string status = "Enter to stage a value, Ctrl+S to save, ESC to return.";
    EditorState st{0,0};

    for (;;) {
        const Size term   = con.size();
        const int  margin = 1;
        const int  header_h = 2;
        const int  footer_h = 1;
        const int  frame_l  = margin;
        const int  frame_t  = header_h + margin;
        const int  frame_w  = std::max(40, term.cols - 2*margin);
        const int  frame_h  = std::max(8,  term.rows - header_h - footer_h - 2*margin);

        const auto& F = area.fields();
        int total = static_cast<int>(F.size());
        if (total <= 0) { status = "No fields."; }

        st.selected = std::clamp(st.selected, 0, std::max(0, total - 1));
        int inner_h = frame_h - 2;
        if (st.selected < st.scroll) st.scroll = st.selected;
        if (st.selected >= st.scroll + inner_h) st.scroll = st.selected - inner_h + 1;

        render_editor(con, term, frame_l, frame_t, frame_w, frame_h, area, st, staged, status);

        Key k = read_key(con);
        if (k == Key::Esc || k == Key::Quit) {
            if (!staged.empty()) {
                const Size t = con.size();
                bool yes = false;
                if (prompt_yes_no(con, t.rows-1, t.cols, "Save changes?", yes)) {
                    if (yes) {
                        bool ok = save_staged_transactional(con, area, staged);
                        if (!ok) { status = "Save failed."; std::cout << '\a'; }
                        else { staged.clear(); status = "Saved."; }
                    } else {
                        staged.clear();
                        status = "Changes discarded.";
                    }
                } else {
                    continue; // canceled prompt
                }
            }
            break;
        }
        switch (k) {
            case Key::Up:
                if (st.selected > 0) { st.selected--; if (st.selected < st.scroll) st.scroll = st.selected; }
                else std::cout << '\a';
                break;
            case Key::Down:
                if (st.selected + 1 < total) { st.selected++; if (st.selected >= st.scroll + inner_h) st.scroll++; }
                else std::cout << '\a';
                break;
            case Key::Home: st.selected = 0; st.scroll = 0; break;
            case Key::End:  st.selected = std::max(0, total - 1); st.scroll = std::max(0, total - inner_h); break;
            case Key::CtrlS: {
                if (staged.empty()) { status = "Nothing to save."; break; }
                bool ok = save_staged_transactional(con, area, staged);
                if (ok) { staged.clear(); status = "Saved."; }
                else    { status = "Save failed."; std::cout << '\a'; }
            } break;
            case Key::Enter: {
                if (st.selected >= 0 && st.selected < total) {
                    const auto& fd   = F[static_cast<size_t>(st.selected)];
                    const std::string fname = fd.name;
                    const char ftype = fd.type;
                    const int flen  = static_cast<int>(fd.length);
                    const int fdec  = static_cast<int>(fd.decimals);

                    std::string normalized;
                    const Size t = con.size();
                    if (!prompt_value_for_field(con, t.rows - 1, t.cols, area, st.selected, fname, ftype, flen, fdec, normalized)) {
                        status = "Edit canceled.";
                        break;
                    }
                    staged.by_name[fname] = normalized;
                    status = "Staged " + fname + ".";
                }
            } break;
            default: break;
        }
    }
}

// -----------------------------
// Simple modals (LIST etc.)
// -----------------------------
static void show_modal(Console& con, const std::string& title,
                       xbase::DbArea& area,
                       void(*handler)(xbase::DbArea&, std::istringstream&),
                       std::istringstream& args) {
    con.clear();
    const Size term = con.size();
    draw_color_bar(con, 0, term.cols, title, "", SGR_HEADER_BAR);
    con.draw_text(0, 2, "");
    handler(area, args);
    std::string hint = "Press any key to return.";
    con.draw_text(0, term.rows - 1, std::string(SGR_FOOTER_BAR) + hint + SGR_RESET);
    std::cout.flush();
    (void)con.get_key();
}

// -----------------------------
// TUI app
// -----------------------------
void cmd_BROWSETUI(xbase::DbArea& area, std::istringstream& iss) {
    (void)iss;

    std::unique_ptr<Console> con(make_console());
    std::string status = "Ready.";
    bool isFullScreen = false;

    PendingEdits staged;

    // Smaller centered window in windowed mode; fullscreen unchanged
    auto render = [&](const std::string& footer_msg){
        const Size term = con->size();

        const bool isFull  = isFullScreen;
        const int  header_h = isFull ? 1 : 3;   // 1 header line in fullscreen, 2+menu in windowed
        const int  footer_h = 1;

        int frame_l = 0, frame_t = 0, frame_w = 0, frame_h = 0;

        if (isFull) {
            const int margin = 0;
            frame_l = margin;
            frame_t = header_h + margin;
            frame_w = std::max(40, term.cols - 2 * margin);
            frame_h = std::max(8,  term.rows - header_h - footer_h - 2 * margin);
        } else {
            const int min_w = 60;
            const int min_h = 12;

            const int target_w = std::clamp(static_cast<int>(term.cols * 0.75), min_w, term.cols - 4);
            const int target_h = std::clamp(static_cast<int>(term.rows * 0.75), min_h,
                                            term.rows - header_h - footer_h - 2);

            frame_w = target_w;
            frame_h = target_h;

            // center inside the content area under header / above footer
            frame_l = std::max(0, (term.cols - frame_w) / 2);
            const int content_rows = term.rows - header_h - footer_h;
            frame_t = header_h + std::max(1, (content_rows - frame_h) / 2);
        }

        const int inner_w = frame_w - 2;
        const int inner_h = frame_h - 2;

        // ---- chrome ----
        con->clear();
        draw_color_bar(*con, 0, term.cols, "DotTalk++", "A TUI Database Shell", SGR_HEADER_BAR);
        if (!isFull) draw_header_menu(*con, 1, term.cols);
        con->draw_frame(frame_l, frame_t, frame_w, frame_h);

        // ---- content ----
        const int recno = area.isOpen() ? static_cast<int>(area.recno()) : 0;
        auto fields = area.isOpen() ? collect_field_views_with_staged(area, staged)
                                    : std::vector<FieldView>();
        auto lines  = build_browse_lines(inner_w, inner_h, recno, fields);
        for (int i = 0; i < inner_h && i < static_cast<int>(lines.size()); ++i) {
            con->draw_text(frame_l + 1, frame_t + 1 + i, lines[static_cast<size_t>(i)], inner_w);
        }

        std::string foot = footer_msg;
        if (!staged.empty()) foot += "  [modified]";
        if (static_cast<int>(foot.size()) < term.cols)
            foot.append(static_cast<size_t>(term.cols - static_cast<int>(foot.size())), ' ');
        con->draw_text(0, term.rows - 1, std::string(SGR_FOOTER_BAR) + foot + SGR_RESET);
        std::cout.flush();
    };

    auto maybe_save_staged = [&](bool navigating)->bool {
        if (staged.empty()) return true;
        const Size term = con->size();
        bool yes = false;
        std::string q = navigating ? "Save changes to this record before navigating?" : "Save changes before exit?";
        if (!prompt_yes_no(*con, term.rows - 1, term.cols, q, yes)) return false;
        if (yes) {
            bool ok = save_staged_transactional(*con, area, staged);
            if (!ok) { status = "Save failed."; std::cout << '\a'; return false; }
            staged.clear();
            status = "Saved.";
        } else {
            staged.clear();
            status = "Changes discarded.";
        }
        return true;
    };

    render(status);

    for (;;) {
        Key k = read_key(*con);

        if (k == Key::Esc || k == Key::Quit) {
            if (!maybe_save_staged(false)) { render(status); continue; }
            status = "Leaving BROWSE.";
            render(status);
            std::cout << "\n";
            break;
        }

        const int total = area.isOpen() ? area.recCount() : 0;
        int cur = get_current_recno_safe(area);

        switch (k) {
            case Key::F6: isFullScreen = !isFullScreen; break;

            case Key::F1: { // Create
                if (!maybe_save_staged(true)) { render(status); continue; }
                const Size term = con->size();
                std::string tableName, fieldSpec;
                if (prompt_for_create_string(*con, term.rows - 1, term.cols, "CREATE table name: ", tableName)) {
                    if (prompt_for_create_string(*con, term.rows - 1, term.cols, "CREATE field spec (e.g. NAME C(20)): ", fieldSpec)) {
                        std::ostringstream ss_cmd;
                        ss_cmd << tableName << " (" << fieldSpec << ")";
                        std::istringstream s(ss_cmd.str());
                        cmd_CREATE(area, s);
                        status = "CREATE done.";
                    } else status = "Create canceled.";
                } else status = "Create canceled.";
            } break;

            case Key::F2:
            case Key::D:
            case Key::L: { // Display/List
                if (!maybe_save_staged(true)) { render(status); continue; }
                std::istringstream noargs;
                show_modal(*con, "LIST/DISPLAY (Press any key)", area, cmd_LIST, noargs);
                status = "Returned from LIST.";
            } break;

            case Key::F3:
            case Key::E: { // Editor
                if (!maybe_save_staged(true)) { render(status); continue; }
                if (!area.isOpen()) { status = "No table open."; break; }
                run_editor(*con, area, staged);
                status = "Returned from EDIT.";
            } break;

            case Key::F4: { // Delete current
                if (!maybe_save_staged(true)) { render(status); continue; }
                if (!area.isOpen()) { status = "No table open."; break; }
                std::istringstream s("DELETE");
                cmd_DELETE(area, s);
                status = "Deleted (if possible).";
            } break;

            case Key::F5: { // Append blank
                if (!maybe_save_staged(true)) { render(status); continue; }
                if (!area.isOpen()) { status = "No table open."; break; }
                std::istringstream s("APPEND BLANK");
                cmd_APPEND(area, s);
                status = "Appended blank.";
            } break;

            case Key::CtrlS: {
                if (staged.empty()) { status = "Nothing to save."; break; }
                bool ok = save_staged_transactional(*con, area, staged);
                if (ok) { staged.clear(); status = "Saved."; }
                else    { status = "Save failed."; std::cout << '\a'; }
            } break;

            case Key::Up: {
                if (total <= 0) { status = "No file open."; break; }
                if (!maybe_save_staged(true)) { break; }
                int target = std::max(1, cur - 1);
                if (target != cur) {
                    std::istringstream s(std::to_string(target));
                    cmd_GOTO(area, s);
                    staged.clear();
                } else std::cout << '\a';
                status = "REC " + std::to_string(get_current_recno_safe(area)) + " / " + std::to_string(total);
            } break;

            case Key::Down: {
                if (total <= 0) { status = "No file open."; break; }
                if (!maybe_save_staged(true)) { break; }
                int target = std::min(total, cur + 1);
                if (target != cur) {
                    std::istringstream s(std::to_string(target));
                    cmd_GOTO(area, s);
                    staged.clear();
                } else std::cout << '\a';
                status = "REC " + std::to_string(get_current_recno_safe(area)) + " / " + std::to_string(total);
            } break;

            case Key::PageUp: {
                if (total <= 0) { status = "No file open."; break; }
                if (!maybe_save_staged(true)) { break; }
                const int pageSize = (con->size().rows - 4) - 2;
                int target = std::max(1, cur - pageSize);
                if (target != cur) {
                    std::istringstream s(std::to_string(target));
                    cmd_GOTO(area, s);
                    staged.clear();
                } else std::cout << '\a';
                status = "REC " + std::to_string(get_current_recno_safe(area)) + " / " + std::to_string(total);
            } break;

            case Key::PageDown: {
                if (total <= 0) { status = "No file open."; break; }
                if (!maybe_save_staged(true)) { break; }
                const int pageSize = (con->size().rows - 4) - 2;
                int target = std::min(total, cur + pageSize);
                if (target != cur) {
                    std::istringstream s(std::to_string(target));
                    cmd_GOTO(area, s);
                    staged.clear();
                } else std::cout << '\a';
                status = "REC " + std::to_string(get_current_recno_safe(area)) + " / " + std::to_string(total);
            } break;

            case Key::Home: {
                if (!maybe_save_staged(true)) { break; }
                std::istringstream s;
                cmd_TOP(area, s);
                staged.clear();
                status = "Top. REC " + std::to_string(get_current_recno_safe(area)) + " / " + std::to_string(total);
            } break;

            case Key::End: {
                if (!maybe_save_staged(true)) { break; }
                std::istringstream s;
                cmd_BOTTOM(area, s);
                staged.clear();
                status = "Bottom. REC " + std::to_string(get_current_recno_safe(area)) + " / " + std::to_string(total);
            } break;

            case Key::G: {
                const Size term = con->size();
                int target = 0;
                if (prompt_goto(*con, term.rows - 1, term.cols, target)) {
                    if (total > 0) {
                        if (!maybe_save_staged(true)) { break; }
                        target = std::clamp(target, 1, total);
                        std::istringstream s(std::to_string(target));
                        cmd_GOTO(area, s);
                        staged.clear();
                        status = "REC " + std::to_string(get_current_recno_safe(area)) + " / " + std::to_string(total);
                    } else status = "No file open.";
                } else status = "Goto canceled.";
            } break;

            case Key::Help:
                status = "↑/↓ prev/next | PgUp/PgDn | Home/End | G goto | L list | E edit | Ctrl+S save | F6 fullscreen | Esc quit";
                break;

            default: break;
        }
        render(status);
    }
}
