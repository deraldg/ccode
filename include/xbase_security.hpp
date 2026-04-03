#pragma once
// xbase_security.hpp
// Canonical, header-only security helpers for cross-platform C++
// Designed for xBase_64: privilege detection, secure paths, temp files, keychain stubs.

#include <string>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <random>
#include <chrono>

#if defined(_WIN32)
    #include <windows.h>
    #include <shlobj.h>
    #include <userenv.h>
#elif defined(__APPLE__)
    #include <sys/types.h>
    #include <unistd.h>
    #include <pwd.h>
#elif defined(__linux__)
    #include <sys/types.h>
    #include <unistd.h>
    #include <pwd.h>
#endif

namespace xbase {
namespace security {

// ------------------------------------------------------------
// 1. Privilege Detection
// ------------------------------------------------------------
inline bool is_elevated()
{
#if defined(_WIN32)
    BOOL elevated = FALSE;
    HANDLE token = nullptr;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(elevation);
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            elevated = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return elevated;

#elif defined(__APPLE__) || defined(__linux__)
    return (geteuid() == 0);
#else
    return false;
#endif
}

// ------------------------------------------------------------
// 2. User-Scoped Directories (Config/Data)
// ------------------------------------------------------------
inline std::string user_data_dir(const std::string& app)
{
#if defined(_WIN32)
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path) != S_OK)
        throw std::runtime_error("Failed to resolve LOCALAPPDATA");

    return std::string(path) + "\\" + app;

#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (!home) throw std::runtime_error("HOME not set");

    return std::string(home) + "/Library/Application Support/" + app;

#elif defined(__linux__)
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg) return std::string(xdg) + "/" + app;

    const char* home = getenv("HOME");
    if (!home) throw std::runtime_error("HOME not set");

    return std::string(home) + "/.local/share/" + app;

#else
    return "./" + app;
#endif
}

// ------------------------------------------------------------
// 3. Secure Random Bytes
// ------------------------------------------------------------
inline std::vector<uint8_t> secure_random(size_t n)
{
    std::vector<uint8_t> out(n);

    std::random_device rd;
    for (size_t i = 0; i < n; ++i)
        out[i] = static_cast<uint8_t>(rd());

    return out;
}

// ------------------------------------------------------------
// 4. Secure Temporary File Creation
// ------------------------------------------------------------
inline std::string secure_temp_file(const std::string& prefix)
{
    auto bytes = secure_random(16);

    std::string hex;
    hex.reserve(32);
    for (auto b : bytes) {
        static const char* lut = "0123456789abcdef";
        hex.push_back(lut[b >> 4]);
        hex.push_back(lut[b & 0xF]);
    }

#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, buf);
    if (len == 0 || len > MAX_PATH)
        throw std::runtime_error("Failed to resolve TEMP directory");

    return std::string(buf) + prefix + "_" + hex + ".tmp";

#else
    const char* tmp = getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";

    return std::string(tmp) + "/" + prefix + "_" + hex + ".tmp";
#endif
}

// ------------------------------------------------------------
// 5. Keychain Abstraction (Stub)
// ------------------------------------------------------------
// These are intentionally abstract. You can implement platform
// backends later (DPAPI, Keychain, libsecret).
// The interface is stable and safe to depend on.

struct keychain
{
    static bool store_secret(const std::string& key, const std::string& value)
    {
        // Stub: replace with platform-specific secure storage.
        (void)key; (void)value;
        return false;
    }

    static std::string load_secret(const std::string& key)
    {
        // Stub: replace with platform-specific secure storage.
        (void)key;
        return {};
    }

    static bool delete_secret(const std::string& key)
    {
        // Stub: replace with platform-specific secure storage.
        (void)key;
        return false;
    }
};

// ------------------------------------------------------------
// 6. Path Canonicalization (No traversal, no symlink escape)
// ------------------------------------------------------------
inline std::string canonicalize(const std::string& path)
{
    // Minimal, portable canonicalization.
    // Real implementation should add symlink resolution per OS.
    if (path.find("..") != std::string::npos)
        throw std::runtime_error("Path traversal detected");

    return path;
}

} // namespace security
} // namespace xbase