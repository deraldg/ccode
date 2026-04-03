#pragma once
// xbase_error_codes.hpp
// Canonical cross-platform error codes for DotTalk++ / xBase_64.
// HRESULT-inspired packed layout, but project-local semantics.
//
// Design rules:
//   * ok() is always 0
//   * error codes are packed and stable
//   * bit 31 is set only for severity::error
//   * warnings and non-zero success codes remain representable

#include <cstdint>
#include <string>

namespace xbase {
namespace error {

// ------------------------------------------------------------
// 1. Severity
// ------------------------------------------------------------
enum class severity : std::uint8_t
{
    success = 0,
    warning = 1,
    error   = 2
};

inline const char* to_severity_string(severity s) noexcept
{
    switch (s) {
        case severity::success: return "success";
        case severity::warning: return "warning";
        case severity::error:   return "error";
        default:                return "unknown";
    }
}

// ------------------------------------------------------------
// 2. Facility (subsystem)
// ------------------------------------------------------------
enum class facility : std::uint16_t
{
    general   = 0x0001,
    dbf64     = 0x0002,
    fpt64     = 0x0003,
    security  = 0x0004,
    cli       = 0x0005,
    io        = 0x0006,
    runtime   = 0x0007
};

inline const char* to_facility_string(facility f) noexcept
{
    switch (f) {
        case facility::general:  return "general";
        case facility::dbf64:    return "dbf64";
        case facility::fpt64:    return "fpt64";
        case facility::security: return "security";
        case facility::cli:      return "cli";
        case facility::io:       return "io";
        case facility::runtime:  return "runtime";
        default:                 return "unknown";
    }
}

// ------------------------------------------------------------
// 3. Canonical packed code (32-bit, HRESULT-inspired)
// ------------------------------------------------------------
// Layout:
//   31      : failure bit (set for severity::error only)
//   30..29  : severity (2 bits)
//   28..16  : facility (13 bits)
//   15..0   : code number (16 bits)
//
// Special rule:
//   0 means OK.
//
using code_type = std::uint32_t;

struct code
{
    code_type value;

    constexpr code() noexcept : value(0) {}
    constexpr explicit code(code_type v) noexcept : value(v) {}

    constexpr severity get_severity() const noexcept
    {
        if (value == 0) {
            return severity::success;
        }
        return static_cast<severity>((value >> 29) & 0x3u);
    }

    constexpr facility get_facility() const noexcept
    {
        if (value == 0) {
            return facility::general;
        }
        return static_cast<facility>((value >> 16) & 0x1FFFu);
    }

    constexpr std::uint16_t get_number() const noexcept
    {
        return static_cast<std::uint16_t>(value & 0xFFFFu);
    }

    constexpr bool ok() const noexcept
    {
        return value == 0 || get_severity() == severity::success;
    }

    constexpr bool failed() const noexcept
    {
        return get_severity() == severity::error;
    }

    friend constexpr bool operator==(code a, code b) noexcept
    {
        return a.value == b.value;
    }

    friend constexpr bool operator!=(code a, code b) noexcept
    {
        return a.value != b.value;
    }
};

// ------------------------------------------------------------
// 4. Packing helpers
// ------------------------------------------------------------
constexpr code ok() noexcept
{
    return code{0};
}

constexpr code make_code(severity sev,
                         facility fac,
                         std::uint16_t num) noexcept
{
    if (sev == severity::success &&
        fac == facility::general &&
        num == 0) {
        return ok();
    }

    code_type v = 0;

    if (sev == severity::error) {
        v |= (1u << 31); // failure bit for errors only
    }

    v |= (static_cast<code_type>(sev) & 0x3u) << 29;
    v |= (static_cast<code_type>(fac) & 0x1FFFu) << 16;
    v |= static_cast<code_type>(num);

    return code{v};
}

// ------------------------------------------------------------
// 5. Canonical error constants (seed set)
// ------------------------------------------------------------

// General
constexpr code e_unknown() noexcept
{
    return make_code(severity::error, facility::general, 0x0001);
}

constexpr code e_invalid_argument() noexcept
{
    return make_code(severity::error, facility::general, 0x0002);
}

constexpr code e_not_implemented() noexcept
{
    return make_code(severity::error, facility::general, 0x0003);
}

// DBF_64
constexpr code e_dbf_header_invalid() noexcept
{
    return make_code(severity::error, facility::dbf64, 0x0001);
}

constexpr code e_dbf_record_out_of_range() noexcept
{
    return make_code(severity::error, facility::dbf64, 0x0002);
}

// FPT64
constexpr code e_fpt_block_invalid() noexcept
{
    return make_code(severity::error, facility::fpt64, 0x0001);
}

// Security
constexpr code e_security_policy_violation() noexcept
{
    return make_code(severity::error, facility::security, 0x0001);
}

constexpr code e_security_elevated_write_forbidden() noexcept
{
    return make_code(severity::error, facility::security, 0x0002);
}

// CLI
constexpr code e_cli_parse_error() noexcept
{
    return make_code(severity::error, facility::cli, 0x0001);
}

// ------------------------------------------------------------
// 6. Message mapping
// ------------------------------------------------------------
inline std::string to_string(code c)
{
    switch (c.value) {
        case 0:
            return "OK";

        case e_unknown().value:
            return "Unknown error";

        case e_invalid_argument().value:
            return "Invalid argument";

        case e_not_implemented().value:
            return "Not implemented";

        case e_dbf_header_invalid().value:
            return "DBF_64 header invalid";

        case e_dbf_record_out_of_range().value:
            return "DBF_64 record out of range";

        case e_fpt_block_invalid().value:
            return "FPT64 block invalid";

        case e_security_policy_violation().value:
            return "Security policy violation";

        case e_security_elevated_write_forbidden().value:
            return "Security: elevated write forbidden";

        case e_cli_parse_error().value:
            return "CLI parse error";

        default:
            return "Unrecognized xBase_64 error code";
    }
}

} // namespace error
} // namespace xbase