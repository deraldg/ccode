#include "area_kind_util.hpp"
#include "xbase.hpp"

namespace xbase {

const char* area_kind_token(AreaKind k) noexcept
{
    switch (k) {
        case AreaKind::V32:  return "v32";
        case AreaKind::V64:  return "v64";
        case AreaKind::V128: return "v128";
        case AreaKind::Tup:  return "tup";
        case AreaKind::Unknown:
        default:             return "unknown";
    }
}

const char* area_kind_label(AreaKind k) noexcept
{
    switch (k) {
        case AreaKind::V32:  return "DBF v32";
        case AreaKind::V64:  return "DBF v64";
        case AreaKind::V128: return "DBF v128";
        case AreaKind::Tup:  return "Tuple";
        case AreaKind::Unknown:
        default:             return "Unknown";
    }
}

} // namespace xbase
