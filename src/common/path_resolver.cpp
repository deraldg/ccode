#include "common/path_resolver.hpp"

#include "cli/cmd_setpath.hpp"
#include "common/path_resolver.hpp"
#include "common/path_state.hpp"
#include <algorithm>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace dottalk::paths {

static bool has_any_sep(const std::string& s) {
    return s.find('/') != std::string::npos || s.find('\\') != std::string::npos;
}

fs::path resolve_in_slot(const fs::path& slot_root, const std::string& token) {
    fs::path p(token);

    if (p.is_absolute()) {
        return p;
    }

    // If token already contains separators, treat it as relative to DATA root,
    // so "dbf/students.dbf" works regardless of current working directory.
    if (has_any_sep(token)) {
        return fs::absolute(state().data_root / p);
    }

    return fs::absolute(slot_root / p);
}

fs::path ensure_ext(fs::path p, const std::string& ext_with_dot) {
    if (!p.has_extension()) {
        p.replace_extension(ext_with_dot);
    }
    return p;
}

fs::path resolve_dbf(const std::string& token) {
    const fs::path root = get_slot(Slot::DBF);
    const fs::path p = resolve_in_slot(root, token);
    return ensure_ext(p, ".dbf");
}

fs::path resolve_index(const std::string& token) {
    const fs::path root = get_slot(Slot::INDEXES);
    const fs::path p = resolve_in_slot(root, token);
    // Public index container/file root:
    //   .inx, .cnx, .cdx, .idx
    // Do not force an extension here.
    return p;
}

fs::path resolve_lmdb_root() {
    return get_slot(Slot::LMDB);
}

fs::path resolve_lmdb_env_for_cdx(const fs::path& public_cdx_path) {
    const fs::path root = get_slot(Slot::LMDB);

    // Derive backend env from public CDX container filename only.
    // Example:
    //   data\indexes\students.cdx
    // -> data\lmdb\students.cdx.d
    fs::path name = public_cdx_path.filename();
    if (name.empty()) {
        name = fs::path("table.cdx");
    }

    return fs::absolute(root / fs::path(name.string() + ".d"));
}

fs::path resolve_workspace(const std::string& token) {
    const fs::path root = get_slot(Slot::WORKSPACES);
    return resolve_in_slot(root, token);
}

fs::path resolve_test(const std::string& token) {
    const fs::path root = get_slot(Slot::TESTS);
    const fs::path p = resolve_in_slot(root, token);
    return ensure_ext(p, ".dts");
}

fs::path resolve_schema(const std::string& token) {
    const fs::path root = get_slot(Slot::SCHEMAS);
    return resolve_in_slot(root, token);
}

fs::path resolve_script(const std::string& token) {
    const fs::path root = get_slot(Slot::SCRIPTS);
    return resolve_in_slot(root, token);
}

fs::path resolve_project(const std::string& token) {
    const fs::path root = get_slot(Slot::PROJECTS);
    return resolve_in_slot(root, token);
}

} // namespace dottalk::paths