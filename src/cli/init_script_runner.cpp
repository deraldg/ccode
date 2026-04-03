#include "cli/init_script_runner.hpp"
#include "cli/script_reader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <exception>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#elif defined(__linux__)
#  include <unistd.h>
#endif

#include "xbase.hpp"

// Direct declaration of the existing shell executor.
// No separate header required.
bool shell_execute_line(xbase::DbArea& area, const std::string& rawLine);

namespace fs = std::filesystem;

namespace dottalk::startup {

std::filesystem::path get_executable_dir() {
#if defined(_WIN32)
    std::wstring buf(MAX_PATH, L'\0');
    DWORD len = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    while (len == buf.size()) {
        buf.resize(buf.size() * 2);
        len = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    }
    if (len > 0) {
        buf.resize(len);
        return fs::path(buf).parent_path();
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size + 1, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) == 0) {
        return fs::weakly_canonical(fs::path(buf.data())).parent_path();
    }
#elif defined(__linux__)
    std::vector<char> buf(4096, '\0');
    const ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (len > 0) {
        buf[static_cast<std::size_t>(len)] = '\0';
        return fs::weakly_canonical(fs::path(buf.data())).parent_path();
    }
#endif
    return fs::current_path();
}

static void strip_utf8_bom(std::string& s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
}

void run_script_file(xbase::DbArea& current, const fs::path& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        std::cout << "SCRIPT: unable to open " << file_path.string() << "\n";
        return;
    }

    std::string line;
    std::size_t line_no = 0;

    while (read_script_command(in, line)) {
        ++line_no;

        if (line_no == 1) {
            strip_utf8_bom(line);
        }

        if (line.empty()) {
            continue;
        }

        try {
            (void)shell_execute_line(current, line);
        } catch (const std::exception& ex) {
            std::cout << "SCRIPT: " << file_path.filename().string()
                      << ":" << line_no << ": " << ex.what() << "\n";
        } catch (...) {
            std::cout << "SCRIPT: " << file_path.filename().string()
                      << ":" << line_no << ": unknown error\n";
        }
    }
}

} // namespace dottalk::startup