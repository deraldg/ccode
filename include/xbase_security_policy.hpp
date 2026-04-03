#pragma once
// xbase_security_policy.hpp
// Canonical, header-only security policy definitions for xBase_64.
// This file defines the rules, expectations, and required behaviors
// for secure operation across Windows, macOS, and Linux.

#include <string>
#include <stdexcept>

namespace xbase {
namespace security {
namespace policy {

// ------------------------------------------------------------
// 1. Security Levels
// ------------------------------------------------------------
enum class level : uint8_t
{
    strict,     // Fail-fast, no unsafe operations, no implicit trust.
    standard,   // Safe defaults, explicit opt-in for risky operations.
    permissive  // Allows legacy/unsafe operations (not recommended).
};

constexpr const char* to_string(level L)
{
    switch (L) {
        case level::strict:     return "strict";
        case level::standard:   return "standard";
        case level::permissive: return "permissive";
    }
    return "unknown";
}

// ------------------------------------------------------------
// 2. Policy Configuration Structure
// ------------------------------------------------------------
struct config
{
    level security_level = level::standard;

    bool allow_network = false;          // No implicit networking.
    bool allow_unsafe_paths = false;     // No traversal, no symlink escape.
    bool allow_plaintext_secrets = false;// Secrets must use OS keychain.
    bool allow_elevated_writes = false;  // Elevated mode must not write user data.
    bool allow_legacy_dbf = false;       // Legacy DBF formats may be unsafe.

    bool require_tty_for_prompts = true; // No secret prompts in non-interactive mode.
    bool require_atomic_writes = true;   // DBF_64/FPT64 structural updates must be atomic.
    bool require_header_validation = true;// DBF/FPT headers must be validated before use.
    bool require_strict_bounds = true;   // Reject invalid field lengths, offsets, memo refs.

    // Convenience: produce a human-readable summary.
    std::string describe() const
    {
        return std::string("xBase_64 Security Policy:\n")
            + "  level: " + to_string(security_level) + "\n"
            + "  allow_network: " + (allow_network ? "yes" : "no") + "\n"
            + "  allow_unsafe_paths: " + (allow_unsafe_paths ? "yes" : "no") + "\n"
            + "  allow_plaintext_secrets: " + (allow_plaintext_secrets ? "yes" : "no") + "\n"
            + "  allow_elevated_writes: " + (allow_elevated_writes ? "yes" : "no") + "\n"
            + "  allow_legacy_dbf: " + (allow_legacy_dbf ? "yes" : "no") + "\n"
            + "  require_tty_for_prompts: " + (require_tty_for_prompts ? "yes" : "no") + "\n"
            + "  require_atomic_writes: " + (require_atomic_writes ? "yes" : "no") + "\n"
            + "  require_header_validation: " + (require_header_validation ? "yes" : "no") + "\n"
            + "  require_strict_bounds: " + (require_strict_bounds ? "yes" : "no") + "\n";
    }
};

// ------------------------------------------------------------
// 3. Predefined Policy Profiles
// ------------------------------------------------------------
constexpr config strict_profile()
{
    return config{
        level::strict,
        false,  // allow_network
        false,  // allow_unsafe_paths
        false,  // allow_plaintext_secrets
        false,  // allow_elevated_writes
        false,  // allow_legacy_dbf
        true,   // require_tty_for_prompts
        true,   // require_atomic_writes
        true,   // require_header_validation
        true    // require_strict_bounds
    };
}

constexpr config standard_profile()
{
    return config{
        level::standard,
        false,  // allow_network
        false,  // allow_unsafe_paths
        false,  // allow_plaintext_secrets
        false,  // allow_elevated_writes
        false,  // allow_legacy_dbf
        true,   // require_tty_for_prompts
        true,   // require_atomic_writes
        true,   // require_header_validation
        true    // require_strict_bounds
    };
}

constexpr config permissive_profile()
{
    return config{
        level::permissive,
        true,   // allow_network
        true,   // allow_unsafe_paths
        true,   // allow_plaintext_secrets
        true,   // allow_elevated_writes
        true,   // allow_legacy_dbf
        false,  // require_tty_for_prompts
        false,  // require_atomic_writes
        false,  // require_header_validation
        false   // require_strict_bounds
    };
}

// ------------------------------------------------------------
// 4. Policy Enforcement Helpers
// ------------------------------------------------------------
inline void enforce(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error("Security policy violation: " + message);
}

inline void enforce_header_validation(const config& cfg)
{
    enforce(cfg.require_header_validation,
            "DBF_64/FPT64 header validation is required.");
}

inline void enforce_atomic_writes(const config& cfg)
{
    enforce(cfg.require_atomic_writes,
            "Atomic writes are required for structural updates.");
}

inline void enforce_no_plaintext_secrets(const config& cfg)
{
    enforce(!cfg.allow_plaintext_secrets,
            "Plaintext secrets are not permitted.");
}

inline void enforce_no_unsafe_paths(const config& cfg)
{
    enforce(!cfg.allow_unsafe_paths,
            "Unsafe paths or traversal are not permitted.");
}

inline void enforce_no_elevated_writes(const config& cfg, bool elevated)
{
    if (elevated)
        enforce(!cfg.allow_elevated_writes,
                "Elevated writes are not permitted under this policy.");
}

} // namespace policy
} // namespace security
} // namespace xbase