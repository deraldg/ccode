
#include "workspace/workarea_manager.hpp"

extern "C" xbase::XBaseEngine* shell_engine();

namespace dottalk::workspace {

int WorkAreaManager::current_slot() const noexcept
{
    auto* eng = shell_engine();
    if (!eng) return -1;
    return eng->currentArea();
}

bool WorkAreaManager::select(int slot)
{
    auto* eng = shell_engine();
    if (!eng) return false;
    eng->selectArea(slot);
    return true;
}

xbase::DbArea* WorkAreaManager::dbarea(int slot) noexcept
{
    auto* eng = shell_engine();
    if (!eng) return nullptr;
    return &eng->area(slot);
}

const xbase::DbArea* WorkAreaManager::dbarea(int slot) const noexcept
{
    auto* eng = shell_engine();
    if (!eng) return nullptr;
    return &eng->area(slot);
}

xbase::DbArea* WorkAreaManager::current_dbarea() noexcept
{
    auto* eng = shell_engine();
    if (!eng) return nullptr;
    return &eng->area(eng->currentArea());
}

const xbase::DbArea* WorkAreaManager::current_dbarea() const noexcept
{
    auto* eng = shell_engine();
    if (!eng) return nullptr;
    return &eng->area(eng->currentArea());
}

}
