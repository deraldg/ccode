#include "workspace/workarea_slot.hpp"

#include <exception>
#include <string>
#include <type_traits>
#include <utility>

#include "xbase.hpp"

extern "C" xbase::XBaseEngine* shell_engine();

namespace
{

template <typename T>
using has_setLogicalName_t = decltype(std::declval<T&>().setLogicalName(std::declval<std::string>()));
template <typename T, typename = has_setLogicalName_t<T>>
static inline void setLogicalNameIf(T& a, const std::string& s, int) { a.setLogicalName(s); }
template <typename T>
static inline void setLogicalNameIf(T&, const std::string&, long) {}

template <typename T>
using has_setName_t = decltype(std::declval<T&>().setName(std::declval<std::string>()));
template <typename T, typename = has_setName_t<T>>
static inline void setLegacyNameIf(T& a, const std::string& s, int) { a.setName(s); }
template <typename T>
static inline void setLegacyNameIf(T&, const std::string&, long) {}

static inline bool open_truth(const xbase::DbArea& a)
{
    try
    {
        return !a.filename().empty();
    }
    catch (...)
    {
        return false;
    }
}

} // namespace

namespace dottalk::workspace
{

WorkAreaSlot::WorkAreaSlot(int slot_number)
    : slot_(slot_number)
{
}

int WorkAreaSlot::slot_number() const noexcept
{
    return slot_;
}

void WorkAreaSlot::set_slot_number(int slot_number) noexcept
{
    slot_ = slot_number;
}

bool WorkAreaSlot::is_open() const noexcept
{
    const xbase::DbArea* a = dbarea();
    return a ? open_truth(*a) : false;
}

bool WorkAreaSlot::is_empty() const noexcept
{
    return !is_open();
}

std::string WorkAreaSlot::alias() const
{
    const xbase::DbArea* a = dbarea();
    if (a)
    {
        try
        {
            const std::string logical = a->logicalName();
            if (!logical.empty())
            {
                return logical;
            }
        }
        catch (...)
        {
        }
    }

    return alias_cache_;
}

void WorkAreaSlot::set_alias(const std::string& alias)
{
    alias_cache_ = alias;

    xbase::DbArea* a = dbarea();
    if (!a)
    {
        return;
    }

    setLogicalNameIf(*a, alias, 0);
    setLegacyNameIf(*a, alias, 0);
}

xbase::DbArea* WorkAreaSlot::dbarea() noexcept
{
    auto* eng = shell_engine();
    if (!eng)
    {
        return nullptr;
    }

    try
    {
        return &eng->area(slot_);
    }
    catch (...)
    {
        return nullptr;
    }
}

const xbase::DbArea* WorkAreaSlot::dbarea() const noexcept
{
    auto* eng = shell_engine();
    if (!eng)
    {
        return nullptr;
    }

    try
    {
        return &eng->area(slot_);
    }
    catch (...)
    {
        return nullptr;
    }
}

bool WorkAreaSlot::open_table(const std::string& path, std::string* err)
{
    try
    {
        if (path.empty())
        {
            if (err) *err = "open_table: empty path";
            return false;
        }

        xbase::DbArea* a = dbarea();
        if (!a)
        {
            if (err) *err = "open_table: engine slot unavailable";
            return false;
        }

        try { a->close(); } catch (...) {}

        a->open(path);
        a->setFilename(path);

        if (alias_cache_.empty())
        {
            try
            {
                alias_cache_ = a->logicalName();
            }
            catch (...)
            {
            }
        }
        else
        {
            set_alias(alias_cache_);
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        if (err) *err = ex.what();
        return false;
    }
    catch (...)
    {
        if (err) *err = "open_table: unknown exception";
        return false;
    }
}

bool WorkAreaSlot::close_table(std::string* err)
{
    try
    {
        xbase::DbArea* a = dbarea();
        if (!a)
        {
            return true;
        }

        try { a->close(); } catch (...) {}
        try { a->setFilename(""); } catch (...) {}

        return true;
    }
    catch (const std::exception& ex)
    {
        if (err) *err = ex.what();
        return false;
    }
    catch (...)
    {
        if (err) *err = "close_table: unknown exception";
        return false;
    }
}

void WorkAreaSlot::reset() noexcept
{
    alias_cache_.clear();
}

} // namespace dottalk::workspace
