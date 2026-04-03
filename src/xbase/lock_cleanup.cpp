#include "lock_cleanup.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

bool process_alive(int pid)
{
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    return (::kill(pid, 0) == 0);
#endif
}

bool parse_owner(const fs::path& p, int& pid_out)
{
    std::ifstream in(p);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("owner=", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                pid_out = std::stoi(line.substr(pos + 1));
                return true;
            }
        }
    }
    return false;
}

int current_pid()
{
#ifdef _WIN32
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

}

namespace dottalk::locks {

void cleanup_stale_locks(const fs::path& root)
{
    int removed = 0;

    for (auto& e : fs::recursive_directory_iterator(root))
    {
        if (!e.is_regular_file())
            continue;

        const auto name = e.path().filename().string();

        if (name.find(".lock.") == std::string::npos)
            continue;

        int pid = 0;
        if (!parse_owner(e.path(), pid))
        {
            fs::remove(e.path());
            removed++;
            continue;
        }

        if (!process_alive(pid))
        {
            fs::remove(e.path());
            removed++;
        }
    }

    if (removed)
        std::cout << "INIT: removed " << removed << " stale lock(s)\n";
}

void cleanup_owned_locks(const fs::path& root)
{
    int removed = 0;
    int mypid = current_pid();

    for (auto& e : fs::recursive_directory_iterator(root))
    {
        if (!e.is_regular_file())
            continue;

        const auto name = e.path().filename().string();

        if (name.find(".lock.") == std::string::npos)
            continue;

        int pid = 0;
        if (!parse_owner(e.path(), pid))
            continue;

        if (pid == mypid)
        {
            fs::remove(e.path());
            removed++;
        }
    }

    if (removed)
        std::cout << "SHUTDOWN: released " << removed << " lock(s)\n";
}

}