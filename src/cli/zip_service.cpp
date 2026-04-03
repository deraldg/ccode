#include "cli/zip_service.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace dottalk::zip {

std::string backend_name(Backend b)
{
    switch (b) {
    case Backend::PowerShellArchive: return "PowerShellArchive";
    case Backend::PosixZip:          return "PosixZip";
    default:                         return "Unknown";
    }
}

std::string operation_name(Operation op)
{
    switch (op) {
    case Operation::List:    return "List";
    case Operation::Create:  return "Create";
    case Operation::Extract: return "Extract";
    default:                 return "None";
    }
}

fs::path ensure_zip_extension(fs::path p)
{
    if (!p.has_extension() || p.extension() != ".zip")
        p += ".zip";
    return p;
}

Result make_error(Operation op,
                  Backend backend,
                  const std::string& message,
                  const fs::path& archive_path,
                  const fs::path& source_path,
                  const fs::path& target_path)
{
    Result r;
    r.success = false;
    r.exit_code = -1;
    r.operation = op;
    r.backend = backend;
    r.message = message;
    r.archive_path = archive_path;
    r.source_path = source_path;
    r.target_path = target_path;
    return r;
}

bool dir_is_empty_safe(const fs::path& p)
{
    std::error_code ec;
    if (!fs::exists(p, ec) || !fs::is_directory(p, ec))
        return false;
    return fs::directory_iterator(p, ec) == fs::directory_iterator{};
}

bool backend_available_for_list(std::string* why_not)
{
#if defined(_WIN32)
    return detail::win_available_list(why_not);
#else
    return detail::posix_available_list(why_not);
#endif
}

bool backend_available_for_create(std::string* why_not)
{
#if defined(_WIN32)
    return detail::win_available_create(why_not);
#else
    return detail::posix_available_create(why_not);
#endif
}

bool backend_available_for_extract(std::string* why_not)
{
#if defined(_WIN32)
    return detail::win_available_extract(why_not);
#else
    return detail::posix_available_extract(why_not);
#endif
}

Result list_archive(const fs::path& archive_path, Listing* listing)
{
#if defined(_WIN32)
    return detail::win_list_archive(archive_path, listing);
#else
    return detail::posix_list_archive(archive_path, listing);
#endif
}

Result create_archive(const fs::path& archive_path, const fs::path& source_path)
{
#if defined(_WIN32)
    return detail::win_create_archive(archive_path, source_path);
#else
    return detail::posix_create_archive(archive_path, source_path);
#endif
}

Result extract_archive(const fs::path& archive_path, const fs::path& target_path)
{
#if defined(_WIN32)
    return detail::win_extract_archive(archive_path, target_path);
#else
    return detail::posix_extract_archive(archive_path, target_path);
#endif
}

} // namespace dottalk::zip