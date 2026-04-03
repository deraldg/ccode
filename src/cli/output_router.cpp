#include "cli/output_router.hpp"

#include <fstream>
#include <iostream>
#include <mutex>
#include <ostream>
#include <streambuf>
#include <string>
#include <algorithm>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#endif

namespace cli {
namespace {

enum class PagerAction {
    Continue,
    All,
    Quit
};

static bool console_is_interactive()
{
#ifdef _WIN32
    DWORD in_mode = 0;
    DWORD out_mode = 0;
    HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) return false;
    if (!GetConsoleMode(hIn, &in_mode)) return false;
    if (!GetConsoleMode(hOut, &out_mode)) return false;
    return true;
#else
    return ::isatty(STDIN_FILENO) && ::isatty(STDOUT_FILENO);
#endif
}

static int visible_console_rows()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 24;

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return 24;

    const int rows = static_cast<int>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
    return (rows > 0) ? rows : 24;
#else
    struct winsize ws{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        return static_cast<int>(ws.ws_row);
    }
    return 24;
#endif
}

static PagerAction read_pager_action()
{
#ifdef _WIN32
    for (;;) {
        const int ch = _getch();

        if (ch == '\r' || ch == '\n') return PagerAction::Continue;

        // swallow extended key prefix
        if (ch == 0 || ch == 224) {
            (void)_getch();
            continue;
        }

        if (ch == 'a' || ch == 'A') return PagerAction::All;
        if (ch == 'q' || ch == 'Q' || ch == 27) return PagerAction::Quit;

        return PagerAction::Continue;
    }
#else
    termios oldt{};
    if (::tcgetattr(STDIN_FILENO, &oldt) != 0) return PagerAction::Continue;

    termios raw = oldt;
    raw.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return PagerAction::Continue;

    unsigned char ch = 0;
    const ssize_t n = ::read(STDIN_FILENO, &ch, 1);
    (void)::tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (n <= 0) return PagerAction::Continue;
    if (ch == '\r' || ch == '\n') return PagerAction::Continue;
    if (ch == 'a' || ch == 'A') return PagerAction::All;
    if (ch == 'q' || ch == 'Q' || ch == 27) return PagerAction::Quit;

    return PagerAction::Continue;
#endif
}

} // anonymous namespace

struct OutputRouter::Impl {
    // state flags
    bool console_on   = true;
    bool print_on     = false;
    bool alternate_on = false;
    bool talk_on      = false;
    bool echo_on      = false;

    // paging flags
    bool paging_on_global          = false;
    bool paging_this_command       = false;
    bool paging_all_this_command   = false;
    bool paging_abort_this_command = false;
    bool shell_input_interactive   = false;
    int  lines_on_page             = 0;
    int  page_len                  = 22;

    // file sinks
    std::string   print_path;
    std::string   alt_path;
    std::ofstream print_file;
    std::ofstream alt_file;

    // original std::cout streambuf (real console path)
    std::streambuf* console_buf = nullptr;

    // stack for temporary cout redirection
    std::vector<std::streambuf*> cout_redirect_stack;

    struct NullBuf : public std::streambuf {
        int overflow(int ch) override { return ch != EOF ? ch : EOF; }
        std::streamsize xsputn(const char*, std::streamsize n) override { return n; }
    };

    NullBuf      null_buf;
    std::ostream null_stream{&null_buf};

    std::mutex mtx;

    bool should_page_console_locked() const {
        return console_on
            && paging_on_global
            && paging_this_command
            && shell_input_interactive
            && !paging_all_this_command
            && !paging_abort_this_command;
    }

    void recompute_page_len_locked() {
        const int rows = visible_console_rows();
        page_len = std::max(3, rows - 2);
    }

    void write_direct_console_locked(const char* s, std::streamsize n) {
        if (!console_buf || !s || n <= 0) return;
        console_buf->sputn(s, n);
        console_buf->pubsync();
    }

    void write_direct_console_char_locked(char c) {
        if (!console_buf) return;
        console_buf->sputc(c);
        if (c == '\n') console_buf->pubsync();
    }

    void clear_more_prompt_locked() {
        static constexpr const char* wipe =
            "\r                                                                                \r";
        write_direct_console_locked(wipe,
            static_cast<std::streamsize>(std::char_traits<char>::length(wipe)));
    }

    void prompt_more_locked() {
        static constexpr const char* prompt =
            "-- More -- (Enter=Continue, A=All, Q=Quit)";
        write_direct_console_locked(prompt,
            static_cast<std::streamsize>(std::char_traits<char>::length(prompt)));

        switch (read_pager_action()) {
        case PagerAction::Continue:
            break;
        case PagerAction::All:
            paging_all_this_command = true;
            break;
        case PagerAction::Quit:
            paging_abort_this_command = true;
            break;
        }

        clear_more_prompt_locked();
        lines_on_page = 0;
        recompute_page_len_locked();
    }

    void note_newline_and_maybe_pause_locked() {
        if (!should_page_console_locked()) return;
        ++lines_on_page;
        if (lines_on_page >= page_len) {
            prompt_more_locked();
        }
    }

    void write_console_locked(const char* s, std::streamsize n) {
        if (!console_on || !s || n <= 0) return;
        if (paging_abort_this_command) return;

        if (!should_page_console_locked()) {
            write_direct_console_locked(s, n);
            return;
        }

        for (std::streamsize i = 0; i < n; ++i) {
            if (paging_abort_this_command) break;

            const char c = s[i];
            write_direct_console_char_locked(c);

            if (c == '\n') {
                note_newline_and_maybe_pause_locked();
            }
        }
    }

    struct MultiBuf : public std::streambuf {
        Impl* impl;

        explicit MultiBuf(Impl* p) : impl(p) {}

        int overflow(int ch) override {
            if (ch == traits_type::eof()) return traits_type::not_eof(ch);
            const char c = static_cast<char>(ch);

            std::lock_guard<std::mutex> guard(impl->mtx);

            if (impl->paging_abort_this_command) {
                return ch;
            }

            impl->write_console_locked(&c, 1);

            if (impl->print_on && impl->print_file.is_open()) {
                impl->print_file.put(c);
                if (c == '\n') impl->print_file.flush();
            }
            if (impl->alternate_on && impl->alt_file.is_open()) {
                impl->alt_file.put(c);
                if (c == '\n') impl->alt_file.flush();
            }

            return ch;
        }

        std::streamsize xsputn(const char* s, std::streamsize n) override {
            if (!s || n <= 0) return 0;

            std::lock_guard<std::mutex> guard(impl->mtx);

            if (impl->paging_abort_this_command) {
                return n;
            }

            impl->write_console_locked(s, n);

            if (impl->print_on && impl->print_file.is_open()) {
                impl->print_file.write(s, n);
                impl->print_file.flush();
            }
            if (impl->alternate_on && impl->alt_file.is_open()) {
                impl->alt_file.write(s, n);
                impl->alt_file.flush();
            }

            return n;
        }
    };

    MultiBuf     multi_buf{this};
    std::ostream routed_stream{&multi_buf};

    Impl() : console_buf(std::cout.rdbuf()) {}
};

OutputRouter& OutputRouter::instance() {
    static OutputRouter global;
    return global;
}

OutputRouter::OutputRouter() : impl_(std::make_unique<Impl>()) {}
OutputRouter::~OutputRouter() = default;

std::ostream& OutputRouter::out() {
    return impl_->routed_stream;
}

void OutputRouter::talk_line(const std::string& line) {
    std::lock_guard<std::mutex> guard(impl_->mtx);
    if (!impl_->talk_on) return;
    if (!impl_->print_on || !impl_->print_file.is_open()) return;

    impl_->print_file << line << '\n';
    impl_->print_file.flush();
}

void OutputRouter::set_console(bool on)   { std::lock_guard<std::mutex> lk(impl_->mtx); impl_->console_on = on; }
void OutputRouter::set_print(bool on)     { std::lock_guard<std::mutex> lk(impl_->mtx); impl_->print_on = on; }
void OutputRouter::set_alternate(bool on) { std::lock_guard<std::mutex> lk(impl_->mtx); impl_->alternate_on = on; }
void OutputRouter::set_talk(bool on)      { std::lock_guard<std::mutex> lk(impl_->mtx); impl_->talk_on = on; }
void OutputRouter::set_echo(bool on)      { std::lock_guard<std::mutex> lk(impl_->mtx); impl_->echo_on = on; }
void OutputRouter::set_paging(bool on)    { std::lock_guard<std::mutex> lk(impl_->mtx); impl_->paging_on_global = on; }

bool OutputRouter::console_on() const   { std::lock_guard<std::mutex> lk(impl_->mtx); return impl_->console_on; }
bool OutputRouter::print_on() const     { std::lock_guard<std::mutex> lk(impl_->mtx); return impl_->print_on; }
bool OutputRouter::alternate_on() const { std::lock_guard<std::mutex> lk(impl_->mtx); return impl_->alternate_on; }
bool OutputRouter::talk_on() const      { std::lock_guard<std::mutex> lk(impl_->mtx); return impl_->talk_on; }
bool OutputRouter::echo_on() const      { std::lock_guard<std::mutex> lk(impl_->mtx); return impl_->echo_on; }
bool OutputRouter::paging_on() const    { std::lock_guard<std::mutex> lk(impl_->mtx); return impl_->paging_on_global; }

void OutputRouter::begin_shell_command(bool interactive_input) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->shell_input_interactive   = interactive_input && console_is_interactive();
    impl_->paging_this_command       = impl_->paging_on_global && impl_->shell_input_interactive;
    impl_->paging_all_this_command   = false;
    impl_->paging_abort_this_command = false;
    impl_->lines_on_page             = 0;
    impl_->recompute_page_len_locked();
}

void OutputRouter::end_shell_command() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->paging_this_command       = false;
    impl_->paging_all_this_command   = false;
    impl_->paging_abort_this_command = false;
    impl_->shell_input_interactive   = false;
    impl_->lines_on_page             = 0;
}

void OutputRouter::push_cout_redirect() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->cout_redirect_stack.push_back(std::cout.rdbuf());
    std::cout.rdbuf(impl_->routed_stream.rdbuf());
}

void OutputRouter::pop_cout_redirect() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->cout_redirect_stack.empty()) return;
    std::streambuf* prev = impl_->cout_redirect_stack.back();
    impl_->cout_redirect_stack.pop_back();
    std::cout.rdbuf(prev);
}

bool OutputRouter::command_aborted() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->paging_abort_this_command;
}

bool OutputRouter::set_print_to(const std::string& path) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->print_file.close();
    impl_->print_path.clear();

    if (path.empty()) return false;

    impl_->print_file.open(path, std::ios::out | std::ios::app);
    if (!impl_->print_file.is_open()) return false;

    impl_->print_path = path;
    impl_->print_on   = true;
    return true;
}

void OutputRouter::close_print_to() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->print_file.close();
    impl_->print_path.clear();
    impl_->print_on = false;
}

bool OutputRouter::set_alternate_to(const std::string& path) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->alt_file.close();
    impl_->alt_path.clear();

    if (path.empty()) return false;

    impl_->alt_file.open(path, std::ios::out | std::ios::app);
    if (!impl_->alt_file.is_open()) return false;

    impl_->alt_path = path;
    impl_->alternate_on = true;
    return true;
}

void OutputRouter::close_alternate_to() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->alt_file.close();
    impl_->alt_path.clear();
    impl_->alternate_on = false;
}

std::string OutputRouter::print_to_path() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->print_path;
}

std::string OutputRouter::alternate_to_path() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    return impl_->alt_path;
}

} // namespace cli