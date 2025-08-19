#include "xbase.hpp"
#include "textio.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

static bool has_ext_ci(const fs::path& p, const char* ext) {
    auto e = textio::up(p.extension().string());
    return e == textio::up(std::string(ext));
}

void cmd_DIR(xbase::DbArea&, std::istringstream& iss) {
    std::string arg;
    std::getline(iss, arg);
    arg = textio::trim(arg);
    fs::path target = arg.empty() ? fs::current_path() : fs::path(arg);

    std::error_code ec;
    if (!fs::exists(target, ec)) {
        std::cout << "Path not found: " << target.string() << "\n";
        return;
    }

    if (fs::is_regular_file(target, ec)) {
        // Show the single file
        auto fsz = fs::file_size(target, ec);
        auto ftime = fs::last_write_time(target, ec);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - decltype(ftime)::clock::now()
                        + std::chrono::system_clock::now());
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::cout << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M")
                  << "       " << std::setw(12) << static_cast<long long>(fsz)
                  << " " << target.filename().string() << "\n";
        return;
    }

    // Directory listing
    std::cout << "\n Directory of " << fs::absolute(target, ec).string() << "\n\n";

    size_t dirs = 0, files = 0;
    uint64_t bytes = 0;

    for (auto& entry : fs::directory_iterator(target, ec)) {
        const auto& p = entry.path();
        auto ftime = entry.last_write_time(ec);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - decltype(ftime)::clock::now()
                        + std::chrono::system_clock::now());
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);

        if (entry.is_directory(ec)) {
            ++dirs;
            std::cout << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M")
                      << "    <DIR>          " << p.filename().string() << "\n";
        } else if (entry.is_regular_file(ec)) {
            ++files;
            auto sz = entry.file_size(ec);
            bytes += sz;

            // Mark .dbf files with a little tag
            bool is_dbf = has_ext_ci(p, ".dbf");
            std::cout << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M")
                      << "         " << std::setw(12) << static_cast<long long>(sz) << " "
                      << (is_dbf ? "[DBF] " : "") << p.filename().string() << "\n";
        }
    }

    std::cout << "             " << dirs  << " Dir(s)\n";
    std::cout << "             " << files << " File(s)  " << bytes << " bytes\n";
}
