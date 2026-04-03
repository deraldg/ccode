#pragma once
// xbase_security_runtime.hpp
// Canonical runtime enforcement layer for xBase_64.
// Applies security policy rules at DBF_64/FPT64 open/close boundaries,
// validates headers, enforces atomic writes, and prevents unsafe operations.

#include <string>
#include <stdexcept>

#include "xbase_security.hpp"
#include "xbase_security_policy.hpp"

namespace xbase {
namespace security {
namespace runtime {

using policy::config;
using policy::enforce;
using policy::enforce_header_validation;
using policy::enforce_atomic_writes;
using policy::enforce_no_plaintext_secrets;
using policy::enforce_no_unsafe_paths;
using policy::enforce_no_elevated_writes;

// ------------------------------------------------------------
// 1. Runtime Context
// ------------------------------------------------------------
struct context
{
    config policy;
    bool is_elevated = false;

    explicit context(const config& cfg)
        : policy(cfg),
          is_elevated(security::is_elevated())
    {}
};

// ------------------------------------------------------------
// 2. DBF/FPT Header Validation Hook
// ------------------------------------------------------------
// Call this immediately after reading a DBF_64 or FPT64 header.
inline void validate_header(const context& ctx, bool header_ok)
{
    enforce_header_validation(ctx.policy);

    enforce(header_ok,
            "DBF_64/FPT64 header failed validation.");
}

// ------------------------------------------------------------
// 3. Path Safety Hook
// ------------------------------------------------------------
// Call this before opening any DBF/FPT file.
inline std::string secure_path(const context& ctx, const std::string& path)
{
    enforce_no_unsafe_paths(ctx.policy);

    // Canonicalize and reject traversal.
    return security::canonicalize(path);
}

// ------------------------------------------------------------
// 4. Elevated Write Protection
// ------------------------------------------------------------
// Call this before performing any write operation.
inline void check_write_permissions(const context& ctx)
{
    enforce_no_elevated_writes(ctx.policy, ctx.is_elevated);
}

// ------------------------------------------------------------
// 5. Atomic Write Enforcement
// ------------------------------------------------------------
// Call this before any structural DBF/FPT update.
inline void require_atomic(const context& ctx)
{
    enforce_atomic_writes(ctx.policy);
}

// ------------------------------------------------------------
// 6. Secret Handling Enforcement
// ------------------------------------------------------------
// Call this before storing credentials, tokens, or keys.
inline void require_secure_secrets(const context& ctx)
{
    enforce_no_plaintext_secrets(ctx.policy);
}

// ------------------------------------------------------------
// 7. High-Level Open/Close Wrappers
// ------------------------------------------------------------
// These are optional helpers you can call from your DBF_64/FPT64 engine.

inline void on_open_begin(const context& ctx, const std::string& path)
{
    (void)secure_path(ctx, path);
}

inline void on_open_end(const context& ctx, bool header_ok)
{
    validate_header(ctx, header_ok);
}

inline void on_write_begin(const context& ctx)
{
    check_write_permissions(ctx);
    require_atomic(ctx);
}

inline void on_store_secret(const context& ctx)
{
    require_secure_secrets(ctx);
}

} // namespace runtime
} // namespace security
} // namespace xbase