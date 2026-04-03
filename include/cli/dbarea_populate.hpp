#pragma once
// src/cli/dbarea_populate.hpp
// Centralized "populate DbArea metadata" helper.
//
// Call this immediately after a successful DbArea::open().
// It standardizes DbArea's canonical metadata (filename/logicalName/dbfBasename,
// abs path) by re-running the existing DbArea path/name computation.

#include <filesystem>
#include <string>

#include "xbase.hpp"

namespace dottalk::dbarea {

// Populate derived/canonical metadata for an already-open area.
//
// Notes:
// - DbArea already owns the logic to compute logicalName/dbfBasename/etc.
//   (behind setFilename()). This helper simply ensures that logic is invoked
//   consistently from all open paths.
// - Pass the resolved DBF path you actually opened.
inline void populate_after_open(xbase::DbArea& a, const std::filesystem::path& opened_path)
{
    // Prefer absolute, normalized path so all commands display the same thing.
    std::filesystem::path abs = opened_path;
    std::error_code ec;
    abs = std::filesystem::absolute(abs, ec);
    if (ec) abs = opened_path;

    // DbArea::setFilename is the canonical entry point for updating/deriving
    // name/basename/path metadata.
    a.setFilename(abs.string());
}

// Convenience overload when caller only has a string.
inline void populate_after_open(xbase::DbArea& a, const std::string& opened_path)
{
    populate_after_open(a, std::filesystem::path(opened_path));
}

} // namespace dottalk::dbarea
