#pragma once
// xbase_error_codes.hpp
// Canonical cross-platform error codes for xBase_64.
// MS-conformal layout, HRESULT-style packing, stable ABI.

#include <cstdint>
#include <string>

namespace xbase {
namespace error {

// ------------------------------------------------------------
// 1. Severity
// ------------------------------------------------------------
enum class severity : uint8_t
{
    success = 0,
    warning = 1,
    error   = 2
};

// ------------------------------------------------------------
// 2. Facility (subsystem)
// ------------------------------------------------------------
enum class facility : uint16_t
{
    general   = 0x0001,
    dbf64     = 0x0002,
    fpt64     = 0x0003,
    security  = 0x0004,
    cli       = 0x0005,
    io        = 0x0006,
    runtime   = 0x0007
};

// ------------------------------------------------------------
// 3. Canonical Error Code (32-bit, HRESULT-like)
// ------------------------------------------------------------
// Layout (bit positions):
//  31      : sign bit (always 1 for error-style codes)
//  30..29  : severity (2 bits)
//  28..16  : facility (13 bits)
//  15..0   : code (16 bits)
using code_type = std::uint32_t;

struct code
{
    code_type value;

    constexpr code() : value(0) {}
    constexpr explicit code(code_type v) : value(v) {}

    constexpr severity get_severity() const noexcept
    {
        return static_cast<severity>((value >> 29) & 0x3);
    }

    constexpr facility get_facility() const noexcept
    {
        return static_cast<facility>((value >> 16) & 0x1FFF);
    }

    constexpr std::uint16_t get_number() const noexcept
    {
        return static_cast<std::uint16_t>(value & 0xFFFF);
    }

    constexpr bool ok() const noexcept
    {
        return get_severity() == severity::success;
    }
};

// ------------------------------------------------------------
// 4. Packing helpers
// ------------------------------------------------------------
constexpr code make_code(severity sev,
                         facility fac,
                         std::uint16_t num) noexcept
{
    code_type v = 0;
    v |= (1u << 31); // sign bit set for xBase_64 codes
    v |= (static_cast<code_type>(sev) & 0x3u) << 29;
    v |= (static_cast<code_type>(fac) & 0x1FFFu) << 16;
    v |= static_cast<code_type>(num);
    return code{v};
}

// ------------------------------------------------------------
// 5. Canonical error constants
// ------------------------------------------------------------

// General
constexpr code ok() noexcept
{
    return make_code(severity::success, facility::general, 0x0000);
}

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
        case 0: return "OK";

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