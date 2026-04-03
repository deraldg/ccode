#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "xbase/about_info.hpp"

#include <iomanip>
#include <sstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

namespace about_info
{
    namespace
    {
        typedef LONG (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    }

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
    #if defined(_M_X64) || defined(__x86_64__)
        return "x64";
    #elif defined(_M_IX86) || defined(__i386__)
        return "x86";
    #elif defined(_M_ARM64) || defined(__aarch64__)
        return "ARM64";
    #elif defined(_M_ARM) || defined(__arm__)
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
        HMODULE h_mod = GetModuleHandleW(L"ntdll.dll");
        if (!h_mod)
        {
            return "Windows (version unavailable)";
        }

        RtlGetVersionPtr rtl_get_version =
            reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(h_mod, "RtlGetVersion"));

        if (!rtl_get_version)
        {
            return "Windows (version unavailable)";
        }

        RTL_OSVERSIONINFOW vi = {};
        vi.dwOSVersionInfoSize = sizeof(vi);

        if (rtl_get_version(&vi) != 0)
        {
            return "Windows (version unavailable)";
        }

        std::ostringstream oss;
        oss << "Windows "
            << vi.dwMajorVersion << "."
            << vi.dwMinorVersion << "."
            << vi.dwBuildNumber;
        return oss.str();
    }

    unsigned int cpu_logical_count()
    {
        SYSTEM_INFO si = {};
        GetSystemInfo(&si);
        return static_cast<unsigned int>(si.dwNumberOfProcessors);
    }

    unsigned long long installed_ram_bytes()
    {
        MEMORYSTATUSEX ms = {};
        ms.dwLength = sizeof(ms);

        if (!GlobalMemoryStatusEx(&ms))
        {
            return 0;
        }

        return static_cast<unsigned long long>(ms.ullTotalPhys);
    }

    std::string path_root_of(const std::string& path)
    {
        if (path.size() >= 3 &&
            ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
            path[1] == ':' &&
            (path[2] == '\\' || path[2] == '/'))
        {
            return path.substr(0, 3);
        }

        char cwd[MAX_PATH] = {};
        DWORD len = GetCurrentDirectoryA(MAX_PATH, cwd);
        if (len > 0)
        {
            std::string s(cwd, len);
            if (s.size() >= 3)
            {
                return s.substr(0, 3);
            }
        }

        return "C:\\";
    }

    DiskInfo disk_info(const std::string& root_path)
    {
        DiskInfo info;
        info.root_path = root_path;

        ULARGE_INTEGER free_bytes_available = {};
        ULARGE_INTEGER total_number_of_bytes = {};
        ULARGE_INTEGER total_number_of_free_bytes = {};

        if (GetDiskFreeSpaceExA(
                root_path.c_str(),
                &free_bytes_available,
                &total_number_of_bytes,
                &total_number_of_free_bytes))
        {
            info.free_bytes = static_cast<unsigned long long>(total_number_of_free_bytes.QuadPart);
            info.total_bytes = static_cast<unsigned long long>(total_number_of_bytes.QuadPart);
        }

        return info;
    }

    std::string computer_name_str()
    {
        char buffer[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD size = static_cast<DWORD>(sizeof(buffer));

        if (!GetComputerNameA(buffer, &size))
        {
            return "Unknown";
        }

        return std::string(buffer, size);
    }

    std::string local_ipv4()
    {
        WSADATA wsa_data = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            return "Unavailable";
        }

        char hostname[256] = {};
        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            WSACleanup();
            return "Unavailable";
        }

        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &result) != 0)
        {
            WSACleanup();
            return "Unavailable";
        }

        std::string ip = "Unavailable";

        for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
        {
            sockaddr_in* sa = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
            char ip_str[INET_ADDRSTRLEN] = {};

            if (InetNtopA(AF_INET, &(sa->sin_addr), ip_str, static_cast<DWORD>(sizeof(ip_str))) != nullptr)
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
        WSACleanup();
        return ip;
    }

    ConsoleInfo console_size()
    {
        ConsoleInfo info;

        HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h_out == INVALID_HANDLE_VALUE || h_out == nullptr)
        {
            return info;
        }

        CONSOLE_SCREEN_BUFFER_INFO csbi = {};
        if (!GetConsoleScreenBufferInfo(h_out, &csbi))
        {
            return info;
        }

        info.width = static_cast<int>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        info.height = static_cast<int>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

        return info;
    }

    bool vt_enabled()
    {
        HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h_out == INVALID_HANDLE_VALUE || h_out == nullptr)
        {
            return false;
        }

        DWORD mode = 0;
        if (!GetConsoleMode(h_out, &mode))
        {
            return false;
        }

        return (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
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