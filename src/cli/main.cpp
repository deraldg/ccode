// ==============================
// File: src/main.cpp
// ==============================
#include <exception>
#include <fstream>
#include <iostream>
#include <string>

#include "runtime/utf8_init.hpp"

#ifdef _WIN32
#include <windows.h>

static void warn_if_vt_input_enabled(const char* where) {
#ifndef NDEBUG
    DWORD m = 0;
    if (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &m) &&
        (m & ENABLE_VIRTUAL_TERMINAL_INPUT)) {
        std::cerr
            << "[DotTalk++ WARN] VT INPUT ENABLED at " << where
            << " — Foxtalk/CLI key handling will break.\n";
    }
#else
    (void)where;
#endif
}

#ifndef NDEBUG
static void dbg_print_vt_out_mode() {
    DWORD m = 0;
    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &m)) {
        std::cerr << "[DBG] VT_OUT="
                  << ((m & ENABLE_VIRTUAL_TERMINAL_PROCESSING) ? "ON" : "OFF")
                  << "\n";
    }
}
#endif
#endif

int run_shell(); // implemented elsewhere

static void print_usage(const char* exe) {
    std::cerr
        << "Usage:\n"
        << "  " << exe << "                 (interactive)\n"
        << "  " << exe << " --script <file>  (feed file to stdin; prompts may still print)\n"
        << "\n"
        << "Also supported:\n"
        << "  " << exe << " < file.dts\n";
}

int main(int argc, char** argv) {
    dottalk::init_utf8();

#ifdef _WIN32
#ifndef NDEBUG
    dbg_print_vt_out_mode();
#endif
    warn_if_vt_input_enabled("main() after init_utf8");
#endif

    try {
        std::ifstream script;
        std::streambuf* old_cin_buf = nullptr;

        if (argc >= 2) {
            const std::string a1 = argv[1];

            if (a1 == "--help" || a1 == "-h" || a1 == "/?") {
                print_usage(argv[0]);
                return 0;
            }

            if (a1 == "--script") {
                if (argc < 3) {
                    print_usage(argv[0]);
                    return 2;
                }
                const std::string path = argv[2];
                script.open(path, std::ios::in);
                if (!script.is_open()) {
                    std::cerr << "Error: cannot open script: " << path << "\n";
                    return 2;
                }

                // Redirect std::cin to read from script file.
                old_cin_buf = std::cin.rdbuf(script.rdbuf());
            } else {
                std::cerr << "Error: unknown option: " << a1 << "\n";
                print_usage(argv[0]);
                return 2;
            }
        }

        const int rc = run_shell();

        // Restore std::cin (best-effort hygiene).
        if (old_cin_buf) {
            std::cin.rdbuf(old_cin_buf);
        }

        return rc;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}
