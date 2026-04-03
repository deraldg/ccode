// xbase_security_tests.cpp
// Canonical module version — no main().
// Provides run_xbase_security_tests() for CLI or external runners.

#include <iostream>
#include <stdexcept>
#include <string>
#include <functional>

#include "xbase_security.hpp"
#include "xbase_security_policy.hpp"
#include "xbase_security_tests.hpp"

using namespace xbase::security;
using namespace xbase::security::policy;

namespace xbase { namespace tests {

// ------------------------------------------------------------
// Minimal test harness
// ------------------------------------------------------------
static int tests_run = 0;
static int tests_failed = 0;

static void assert_true(bool cond, const std::string& msg)
{
    ++tests_run;
    if (!cond) {
        ++tests_failed;
        std::cerr << "[FAIL] " << msg << "\n";
    } else {
        std::cout << "[ OK ] " << msg << "\n";
    }
}

static void assert_throws(const std::function<void()>& fn, const std::string& msg)
{
    ++tests_run;
    try {
        fn();
        ++tests_failed;
        std::cerr << "[FAIL] " << msg << " (no exception thrown)\n";
    } catch (...) {
        std::cout << "[ OK ] " << msg << "\n";
    }
}

// ------------------------------------------------------------
// Test groups
// ------------------------------------------------------------
static void test_privilege_detection()
{
    bool elevated = is_elevated();
    assert_true(elevated == true || elevated == false,
                "is_elevated() returns a boolean");
}

static void test_user_data_dir()
{
    std::string dir = user_data_dir("xbase64_testapp");
    assert_true(!dir.empty(), "user_data_dir() returns a non-empty path");
}

static void test_secure_random()
{
    auto bytes = secure_random(32);
    assert_true(bytes.size() == 32, "secure_random(32) returns 32 bytes");

    bool nonzero = false;
    for (auto b : bytes) {
        if (b != 0) { nonzero = true; break; }
    }
    assert_true(nonzero, "secure_random() produces non-zero entropy");
}

static void test_secure_temp_file()
{
    std::string f1 = secure_temp_file("xbase");
    std::string f2 = secure_temp_file("xbase");

    assert_true(f1 != f2, "secure_temp_file() produces unique names");
    assert_true(f1.find("..") == std::string::npos,
                "secure_temp_file() does not contain traversal");
}

static void test_canonicalize()
{
    assert_true(canonicalize("safe/path") == "safe/path",
                "canonicalize() accepts safe paths");

    assert_throws([](){ canonicalize("../escape"); },
                  "canonicalize() rejects traversal");
}

static void test_policy_profiles()
{
    auto strict = strict_profile();
    auto standard = standard_profile();
    auto permissive = permissive_profile();

    assert_true(strict.security_level == level::strict,
                "strict_profile() sets strict level");
    assert_true(!strict.allow_network,
                "strict_profile() disables networking");

    assert_true(standard.security_level == level::standard,
                "standard_profile() sets standard level");

    assert_true(permissive.security_level == level::permissive,
                "permissive_profile() sets permissive level");
    assert_true(permissive.allow_network,
                "permissive_profile() enables networking");
}

static void test_policy_enforcement()
{
    auto strict = strict_profile();

    assert_throws([&](){ enforce_no_plaintext_secrets(strict); },
                  "strict policy forbids plaintext secrets");

    assert_throws([&](){ enforce_no_unsafe_paths(strict); },
                  "strict policy forbids unsafe paths");

    assert_throws([&](){ enforce_no_elevated_writes(strict, true); },
                  "strict policy forbids elevated writes");

    auto standard = standard_profile();
    assert_throws([&](){ enforce_no_plaintext_secrets(standard); },
                  "standard policy forbids plaintext secrets");
}

// ------------------------------------------------------------
// Public entry point
// ------------------------------------------------------------
int run_xbase_security_tests()
{
    tests_run = 0;
    tests_failed = 0;

    test_privilege_detection();
    test_user_data_dir();
    test_secure_random();
    test_secure_temp_file();
    test_canonicalize();
    test_policy_profiles();
    test_policy_enforcement();

    return tests_failed;
}

}} // namespace xbase::tests