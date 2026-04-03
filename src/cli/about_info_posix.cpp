#include "xbase/about_info.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace about_info
{
    std::string app_name()
    {
        return "DotTalk++";
    }

    std::string build_mode()
    {
    #ifdef NDEBUG
        return "Release";
    #else
        return "Debug";
    #endif
    }

    std::string build_date_time()
    {
        return std::string(__DATE__) + " " + __TIME__;
    }

    std::string architecture()
    {
    #if defined(__x86_64__)
        return "x64";
    #elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
        return "x86";
    #elif defined(__aarch64__)
        return "ARM64";
    #elif defined(__arm__)
        return "ARM";
    #else
        return "Unknown";
    #endif
    }

    std::string compiler_string()
    {
    #ifdef _MSC_VER
        std::ostringstream oss;
        oss << "MSVC " << _MSC_VER;
        return oss.str();
    #elif defined(__clang__)
        std::ostringstream oss;
        oss << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
        return oss.str();
    #elif defined(__GNUC__)
        std::ostringstream oss;
        oss << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
        return oss.str();
    #else
        return "Unknown";
    #endif
    }

    std::string cpp_standard_string()
    {
        std::ostringstream oss;
        oss << __cplusplus;
        return oss.str();
    }

    std::string windows_version()
    {
        struct utsname u = {};
        if (uname(&u) == 0)
        {
            std::ostringstream oss;
            oss << u.sysname << " " << u.release;
            return oss.str();
        }

        return "Linux";
    }

    unsigned int cpu_logical_count()
    {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        if (n <= 0)
        {
            return 1;
        }
        return static_cast<unsigned int>(n);
    }

    unsigned long long installed_ram_bytes()
    {
        const long pages = sysconf(_SC_PHYS_PAGES);
        const long page_size = sysconf(_SC_PAGE_SIZE);

        if (pages <= 0 || page_size <= 0)
        {
            return 0;
        }

        return static_cast<unsigned long long>(pages) *
               static_cast<unsigned long long>(page_size);
    }

    std::string path_root_of(const std::string& path)
    {
        if (path.empty())
        {
            return "/";
        }

        if (path.size() >= 7 &&
            path[0] == '/' &&
            path[1] == 'm' &&
            path[2] == 'n' &&
            path[3] == 't' &&
            path[4] == '/' &&
            ((path[5] >= 'A' && path[5] <= 'Z') || (path[5] >= 'a' && path[5] <= 'z')) &&
            path[6] == '/')
        {
            return path.substr(0, 7);
        }

        if (path[0] == '/')
        {
            return "/";
        }

        char cwd[4096] = {};
        if (getcwd(cwd, sizeof(cwd)) != nullptr)
        {
            std::string s(cwd);

            if (s.size() >= 7 &&
                s[0] == '/' &&
                s[1] == 'm' &&
                s[2] == 'n' &&
                s[3] == 't' &&
                s[4] == '/' &&
                ((s[5] >= 'A' && s[5] <= 'Z') || (s[5] >= 'a' && s[5] <= 'z')) &&
                s[6] == '/')
            {
                return s.substr(0, 7);
            }

            return "/";
        }

        return "/";
    }

    DiskInfo disk_info(const std::string& root_path)
    {
        DiskInfo info;
        info.root_path = root_path.empty() ? "/" : root_path;

        struct statvfs s = {};
        if (statvfs(info.root_path.c_str(), &s) == 0)
        {
            info.free_bytes =
                static_cast<unsigned long long>(s.f_bavail) *
                static_cast<unsigned long long>(s.f_frsize);

            info.total_bytes =
                static_cast<unsigned long long>(s.f_blocks) *
                static_cast<unsigned long long>(s.f_frsize);
        }

        return info;
    }

    std::string computer_name_str()
    {
        char host[256] = {};
        if (gethostname(host, sizeof(host)) == 0)
        {
            host[sizeof(host) - 1] = '\0';
            return std::string(host);
        }

        return "Unknown";
    }

    std::string local_ipv4()
    {
        char hostname[256] = {};
        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            return "Unavailable";
        }

        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* result = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &result) != 0)
        {
            return "Unavailable";
        }

        std::string ip = "Unavailable";

        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
        {
            if (ptr->ai_family != AF_INET || ptr->ai_addr == nullptr)
            {
                continue;
            }

            const sockaddr_in* sa = reinterpret_cast<const sockaddr_in*>(ptr->ai_addr);
            char ip_str[INET_ADDRSTRLEN] = {};

            if (inet_ntop(AF_INET, &(sa->sin_addr), ip_str, sizeof(ip_str)) != nullptr)
            {
                std::string candidate = ip_str;

                if (candidate != "127.0.0.1")
                {
                    ip = candidate;
                    break;
                }

                if (ip == "Unavailable")
                {
                    ip = candidate;
                }
            }
        }

        freeaddrinfo(result);
        return ip;
    }

    ConsoleInfo console_size()
    {
        ConsoleInfo info;

        struct winsize ws = {};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
        {
            info.width = static_cast<int>(ws.ws_col);
            info.height = static_cast<int>(ws.ws_row);
        }
        else
        {
            info.width = 80;
            info.height = 25;
        }

        return info;
    }

    bool vt_enabled()
    {
        return true;
    }

    std::string format_bytes(unsigned long long bytes)
    {
        static const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB" };

        double size = static_cast<double>(bytes);
        int unit_index = 0;

        while (size >= 1024.0 && unit_index < 5)
        {
            size /= 1024.0;
            ++unit_index;
        }

        std::ostringstream oss;
        oss << std::fixed
            << std::setprecision(unit_index == 0 ? 0 : 1)
            << size << " " << units[unit_index];
        return oss.str();
    }
}