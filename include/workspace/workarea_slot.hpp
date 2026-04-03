#pragma once

#include <string>

#include "xbase.hpp"

namespace dottalk::workspace
{

class WorkAreaSlot
{
private:
    int slot_ = -1;
    std::string alias_cache_;

public:
    WorkAreaSlot() = default;
    explicit WorkAreaSlot(int slot_number);

    int slot_number() const noexcept;

    void set_slot_number(int slot_number) noexcept;

    bool is_open() const noexcept;
    bool is_empty() const noexcept;

    std::string alias() const;
    void set_alias(const std::string& alias);

    xbase::DbArea* dbarea() noexcept;
    const xbase::DbArea* dbarea() const noexcept;

    bool open_table(
        const std::string& path,
        std::string* err = nullptr);

    bool close_table(
        std::string* err = nullptr);

    void reset() noexcept;
};

} // namespace dottalk::workspace
