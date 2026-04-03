#pragma once

#include <filesystem>
#include <string>

namespace xbase { class DbArea; }

namespace dottalk::startup {

std::filesystem::path get_executable_dir();
void run_script_file(xbase::DbArea& current, const std::filesystem::path& file_path);

} // namespace dottalk::startup