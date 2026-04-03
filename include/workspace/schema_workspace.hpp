#pragma once

#include <string>
#include <vector>

namespace dottalk::workspace {

class WorkAreaManager;

struct SchemaAreaState
{
    int slot = -1;

    std::string path;         // DBF absolute path
    std::string logical;      // logical / alias name

    std::string index_path;   // active order container/path (if any)
    std::string index_type;   // INX / CNX / CDX / NONE / UNKNOWN
    std::string active_tag;   // active tag if any

    int recno = 0;
};

class SchemaWorkspace
{
public:
    int current_slot = 0;
    std::vector<SchemaAreaState> areas;

    bool capture_from_runtime(const WorkAreaManager& wam);
    bool apply_to_runtime(WorkAreaManager& wam);

    bool save_file(const std::string& path) const;
    bool load_file(const std::string& path);
};

} // namespace dottalk::workspace
