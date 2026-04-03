#pragma once

#include <string>

namespace xbase
{
class DbArea;
}

namespace dottalk::workspace
{

class WorkAreaSlot;

struct SchemaAreaState
{
    int slot = -1;
    bool is_open = false;

    std::string dbf_path;
    std::string logical_name;
    std::string alias;

    bool is_x64 = false;

    std::string memo_kind;   // none / dbt / fpt
    std::string memo_path;

    std::string index_container_path;
    std::string active_tag;
    std::string active_order_name;

    int recno = 0;

    bool is_valid(std::string* err = nullptr) const;

    bool capture_from_dbarea(
        int slot_num,
        const xbase::DbArea& area,
        std::string* err = nullptr);

    bool apply_to_slot(
        WorkAreaSlot& slot_ref,
        std::string* err = nullptr) const;
};

} // namespace dottalk::workspace
