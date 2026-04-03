#pragma once

#include <cstdint>

namespace xbase {

enum class AreaKind : std::uint8_t;

// Canonical short token used in diagnostics and metadata.
const char* area_kind_token(AreaKind k) noexcept;

// Human-friendly label if you ever want more descriptive reporting.
const char* area_kind_label(AreaKind k) noexcept;

} // namespace xbase
