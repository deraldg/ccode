#pragma once
// xbase_error_context.hpp
// Canonical engine-wide error context for xBase_64.
// Provides thread-local last-error tracking and RAII guards.

#include "xbase_error_codes.hpp"

namespace xbase {
namespace error {

// -----------------------------------------------------------------------------
// Thread-local last error
// -----------------------------------------------------------------------------
inline thread_local code tls_last_error = ok();

// -----------------------------------------------------------------------------
// Set/Get
// -----------------------------------------------------------------------------
inline void set_last_error(code c) noexcept
{
    tls_last_error = c;
}

inline code get_last_error() noexcept
{
    return tls_last_error;
}

// -----------------------------------------------------------------------------
// Clear
// -----------------------------------------------------------------------------
inline void clear_last_error() noexcept
{
    tls_last_error = ok();
}

// -----------------------------------------------------------------------------
// RAII Guard
// -----------------------------------------------------------------------------
class error_guard
{
public:
    error_guard() noexcept
        : saved_(tls_last_error)
    {
        tls_last_error = ok();
    }

    ~error_guard()
    {
        tls_last_error = saved_;
    }

private:
    code saved_;
};

} // namespace error
} // namespace xbase