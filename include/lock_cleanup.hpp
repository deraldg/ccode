#pragma once
#include <filesystem>
#include <string>

namespace dottalk::locks {

void cleanup_stale_locks(const std::filesystem::path& dbf_root);
void cleanup_owned_locks(const std::filesystem::path& dbf_root);

}