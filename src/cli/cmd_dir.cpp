// src/cli/cmd_dir.cpp
#include "xbase.hpp"
#include "textio.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstdint>

namespace fs = std::filesystem;

static bool has_ext_ci(const fs::path& p, const char* ext) {
    auto e = textio::up(p.extension().string());
    return e == textio::up(std::string(ext));
}

static std::time_t to_time_t(fs::file_time_type ft) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
        ft - fs::file_time_type::clock::now() + system_clock::now()
    );
    return system_clock::to_time_t(sctp);
}

static std::tm to_local_tm(std::time_t tt) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    return tm;
}

static std::string format_commas(std::uint64_t v) {
    std::string s = std::to_string(v);
    std::string out; out.reserve(s.size() + s.size()/3);
    int n = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        if (n == 3) { out.push_back(','); n = 0; }
        out.push_back(*it); ++n;
    }
    std::reverse(out.begin(), out.end());
    return out;
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
        auto fsz   = fs::file_size(target, ec);
        auto ftime = fs::last_write_time(target, ec);
        std::tm tm = to_local_tm(to_time_t(ftime));

        std::cout << std::put_time(&tm, "%Y-%m-%d %H:%M")
                  << "       " << std::setw(12) << format_commas(static_cast<std::uint64_t>(fsz))
                  << " " << target.filename().string() << "\n";
        return;
    }

    std::cout << "\n Directory of " << fs::absolute(target, ec).string() << "\n\n";

    std::size_t dirs = 0, files = 0;
    std::uint64_t bytes = 0;

    for (fs::directory_iterator it(target, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        const auto& entry = *it;
        const auto& p = entry.path();

        auto ftime = entry.last_write_time(ec);
        if (ec) continue;
        std::tm tm = to_local_tm(to_time_t(ftime));

        if (entry.is_directory(ec)) {
            ++dirs;
            std::cout << std::put_time(&tm, "%Y-%m-%d %H:%M")
                      << "    <DIR>          " << p.filename().string() << "\n";
        } else if (entry.is_regular_file(ec)) {
            ++files;
            auto sz = entry.file_size(ec);
            bytes += static_cast<std::uint64_t>(sz);

            bool is_dbf = has_ext_ci(p, ".dbf");
            std::cout << std::put_time(&tm, "%Y-%m-%d %H:%M")
                      << "         " << std::setw(12) << format_commas(static_cast<std::uint64_t>(sz)) << " "
                      << (is_dbf ? "[DBF] " : "") << p.filename().string() << "\n";
        }
    }

    std::cout << "             " << dirs  << " Dir(s)\n";
    std::cout << "             " << files << " File(s)  " << format_commas(bytes) << " bytes\n";
}
