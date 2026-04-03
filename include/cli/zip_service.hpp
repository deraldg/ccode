#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dottalk::zip {

// Backend selected by platform/service layer.
enum class Backend {
    Unknown = 0,
    PowerShellArchive,
    PosixZip
};

// High-level operation kind.
enum class Operation {
    None = 0,
    List,
    Create,
    Extract
};

// Normalized result returned by any backend.
struct Result {
    bool success = false;
    int exit_code = -1;

    Backend backend = Backend::Unknown;
    Operation operation = Operation::None;

    std::filesystem::path archive_path;
    std::filesystem::path source_path;
    std::filesystem::path target_path;

    std::string message;
    std::string stdout_text;
    std::string stderr_text;
};

// Simple listing payload for LIST operations.
struct Listing {
    std::vector<std::string> entries;
};

// Service-level helpers
std::string backend_name(Backend b);
std::string operation_name(Operation op);

// Path helpers
std::filesystem::path ensure_zip_extension(std::filesystem::path p);

// Shared service helpers
Result make_error(Operation op,
                  Backend backend,
                  const std::string& message,
                  const std::filesystem::path& archive_path = {},
                  const std::filesystem::path& source_path = {},
                  const std::filesystem::path& target_path = {});

bool dir_is_empty_safe(const std::filesystem::path& p);

// Generic ZIP service entry points.
Result list_archive(const std::filesystem::path& archive_path, Listing* listing = nullptr);

Result create_archive(const std::filesystem::path& archive_path,
                      const std::filesystem::path& source_path);

Result extract_archive(const std::filesystem::path& archive_path,
                       const std::filesystem::path& target_path);

// Backend capability probes.
bool backend_available_for_list(std::string* why_not = nullptr);
bool backend_available_for_create(std::string* why_not = nullptr);
bool backend_available_for_extract(std::string* why_not = nullptr);

namespace detail {

// POSIX backend
bool posix_available_list(std::string* why_not);
bool posix_available_create(std::string* why_not);
bool posix_available_extract(std::string* why_not);

Result posix_list_archive(const std::filesystem::path& archive_path, Listing* listing);
Result posix_create_archive(const std::filesystem::path& archive_path,
                            const std::filesystem::path& source_path);
Result posix_extract_archive(const std::filesystem::path& archive_path,
                             const std::filesystem::path& target_path);

// Windows backend
bool win_available_list(std::string* why_not);
bool win_available_create(std::string* why_not);
bool win_available_extract(std::string* why_not);

Result win_list_archive(const std::filesystem::path& archive_path, Listing* listing);
Result win_create_archive(const std::filesystem::path& archive_path,
                          const std::filesystem::path& source_path);
Result win_extract_archive(const std::filesystem::path& archive_path,
                           const std::filesystem::path& target_path);

} // namespace detail

} // namespace dottalk::zip