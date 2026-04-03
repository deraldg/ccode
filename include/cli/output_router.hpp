#pragma once
#include <iosfwd>
#include <memory>
#include <string>

namespace cli {

class OutputRouter {
public:
    static OutputRouter& instance();

    std::ostream& out();

    void talk_line(const std::string& line);

    void set_echo(bool on);
    bool echo_on() const;

    void set_console(bool on);
    void set_print(bool on);
    void set_alternate(bool on);
    void set_talk(bool on);

    bool console_on() const;
    bool print_on() const;
    bool alternate_on() const;
    bool talk_on() const;

    // paging control
    void set_paging(bool on);
    bool paging_on() const;

    // per-command shell control
    void begin_shell_command(bool interactive_input);
    void end_shell_command();

    // temporary cout routing
    void push_cout_redirect();
    void pop_cout_redirect();

    bool command_aborted() const;

    bool set_print_to(const std::string& path);
    void close_print_to();

    bool set_alternate_to(const std::string& path);
    void close_alternate_to();

    std::string print_to_path() const;
    std::string alternate_to_path() const;

private:
    OutputRouter();
    ~OutputRouter();

    OutputRouter(const OutputRouter&) = delete;
    OutputRouter& operator=(const OutputRouter&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}