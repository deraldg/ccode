#pragma once
// xbase_error_runtime.hpp
// Runtime helpers for DotTalk++ / xBase_64 error codes.
// Exception wrapper, HRESULT bridge, and convenience utilities.

#include <cstdint>
#include <stdexcept>
#include <string>

#include "xbase_error_codes.hpp"

namespace xbase {
namespace error {

// ------------------------------------------------------------
// Exception type
// ------------------------------------------------------------
class exception : public std::runtime_error
{
public:
    explicit exception(code c, const std::string& detail = {})
        : std::runtime_error(build_message(c, detail)),
          code_(c)
    {}

    code get_code() const noexcept
    {
        return code_;
    }

private:
    code code_;

    static std::string build_message(code c, const std::string& detail)
    {
        std::string msg;
        msg.reserve(128);

        msg += to_severity_string(c.get_severity());
        msg += " [";
        msg += to_facility_string(c.get_facility());
        msg += " ";
        msg += std::to_string(c.get_number());
        msg += "]: ";
        msg += to_string(c);

        if (!detail.empty()) {
            msg += " (";
            msg += detail;
            msg += ")";
        }

        return msg;
    }
};

// ------------------------------------------------------------
// Throw helpers
// ------------------------------------------------------------
[[noreturn]] inline void throw_error(code c,
                                     const std::string& detail = {})
{
    throw exception(c, detail);
}

inline void throw_if_failed(code c,
                            const std::string& detail = {})
{
    if (c.failed()) {
        throw_error(c, detail);
    }
}

// ------------------------------------------------------------
// HRESULT-style bridge
// ------------------------------------------------------------
// Project-local packed layout. This is a passthrough bridge for
// interop, not a claim of full binary HRESULT equivalence.
//
inline std::uint32_t to_hresult(code c) noexcept
{
    return c.value;
}

inline code from_hresult(std::uint32_t hr) noexcept
{
    return code{hr};
}

} // namespace error
} // namespace xbase