#pragma once

#include <filesystem>
#include <string>

namespace dottalk::paths {

namespace fs = std::filesystem;

// ---- declarations only ----

fs::path resolve_in_slot(const fs::path& slot_root, const std::string& token);
fs::path ensure_ext(fs::path p, const std::string& ext_with_dot);

fs::path resolve_dbf(const std::string& token);
fs::path resolve_index(const std::string& token);
fs::path resolve_lmdb_root();
fs::path resolve_lmdb_env_for_cdx(const fs::path& public_cdx_path);
fs::path resolve_workspace(const std::string& token);
fs::path resolve_test(const std::string& token);
fs::path resolve_schema(const std::string& token);
fs::path resolve_script(const std::string& token);
fs::path resolve_project(const std::string& token);

} // namespace dottalk::paths