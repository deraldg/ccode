#pragma once

#include <string>

namespace about_info
{
    struct DiskInfo
    {
        std::string root_path;
        unsigned long long free_bytes = 0;
        unsigned long long total_bytes = 0;
    };

    struct ConsoleInfo
    {
        int width = 0;
        int height = 0;
    };

    std::string app_name();
    std::string build_mode();
    std::string build_date_time();
    std::string architecture();
    std::string compiler_string();
    std::string cpp_standard_string();

    std::string windows_version();
    unsigned int cpu_logical_count();
    unsigned long long installed_ram_bytes();

    DiskInfo disk_info(const std::string& root_path);
    std::string path_root_of(const std::string& path);

    std::string computer_name_str();
    std::string local_ipv4();

    ConsoleInfo console_size();
    bool vt_enabled();

    std::string format_bytes(unsigned long long bytes);
}