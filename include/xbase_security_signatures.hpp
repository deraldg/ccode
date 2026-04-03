#pragma once
// xbase_security_signatures.hpp
// Canonical cross-platform ABI for the xBase_64 security subsystem.
// This header declares all public security-related signatures without
// providing implementations. It serves as the authoritative interface
// for engines, CLI commands, and runtime enforcement layers.

#include <string>
#include <vector>
#include <cstdint>

namespace xbase {
namespace security {

// ------------------------------------------------------------
// 1. Privilege Detection
// ------------------------------------------------------------
bool is_elevated() noexcept;


// ------------------------------------------------------------
// 2. User-Scoped Directories
// ------------------------------------------------------------
std::string user_data_dir(const std::string& app);


// ------------------------------------------------------------
// 3. Secure Random
// ------------------------------------------------------------
std::vector<uint8_t> secure_random(std::size_t n);


// ------------------------------------------------------------
// 4. Secure Temporary Files
// ------------------------------------------------------------
std::string secure_temp_file(const std::string& prefix);


// ------------------------------------------------------------
// 5. Path Canonicalization
// ------------------------------------------------------------
std::string canonicalize(const std::string& path);


// ------------------------------------------------------------
// 6. Keychain Abstraction (Stable Interface)
// ------------------------------------------------------------
struct keychain
{
    static bool store_secret(const std::string& key,
                             const std::string& value);

    static std::string load_secret(const std::string& key);

    static bool delete_secret(const std::string& key);
};

} // namespace security
} // namespace xbase



// ============================================================================
//  POLICY LAYER
// ============================================================================
namespace xbase {
namespace security {
namespace policy {

// ------------------------------------------------------------
// 7. Policy Levels
// ------------------------------------------------------------
enum class level : uint8_t { strict, standard, permissive };

const char* to_string(level L) noexcept;


// ------------------------------------------------------------
// 8. Policy Configuration
// ------------------------------------------------------------
struct config
{
    level security_level;

    bool allow_network;
    bool allow_unsafe_paths;
    bool allow_plaintext_secrets;
    bool allow_elevated_writes;
    bool allow_legacy_dbf;

    bool require_tty_for_prompts;
    bool require_atomic_writes;
    bool require_header_validation;
    bool require_strict_bounds;

    std::string describe() const;
};


// ------------------------------------------------------------
// 9. Policy Profiles
// ------------------------------------------------------------
config strict_profile() noexcept;
config standard_profile() noexcept;
config permissive_profile() noexcept;


// ------------------------------------------------------------
// 10. Policy Enforcement
// ------------------------------------------------------------
void enforce(bool condition, const std::string& message);

void enforce_header_validation(const config& cfg);
void enforce_atomic_writes(const config& cfg);
void enforce_no_plaintext_secrets(const config& cfg);
void enforce_no_unsafe_paths(const config& cfg);
void enforce_no_elevated_writes(const config& cfg, bool elevated);

} // namespace policy
} // namespace security
} // namespace xbase



// ============================================================================
//  RUNTIME LAYER
// ============================================================================
namespace xbase {
namespace security {
namespace runtime {

using policy::config;

// ------------------------------------------------------------
// 11. Runtime Context
// ------------------------------------------------------------
struct context
{
    config policy;
    bool is_elevated;

    explicit context(const config& cfg);
};


// ------------------------------------------------------------
// 12. Runtime Hooks
// ------------------------------------------------------------
void validate_header(const context& ctx, bool header_ok);

std::string secure_path(const context& ctx,
                        const std::string& path);

void check_write_permissions(const context& ctx);

void require_atomic(const context& ctx);

void require_secure_secrets(const context& ctx);


// ------------------------------------------------------------
// 13. High-Level Open/Close Hooks
// ------------------------------------------------------------
void on_open_begin(const context& ctx,
                   const std::string& path);

void on_open_end(const context& ctx,
                 bool header_ok);

void on_write_begin(const context& ctx);

void on_store_secret(const context& ctx);

} // namespace runtime
} // namespace security
} // namespace xbase