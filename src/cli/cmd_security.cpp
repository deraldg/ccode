// src/cli/cmd_SECURITY.cpp
// Security inspection and diagnostics for x64Base.

#include <iostream>
#include <sstream>
#include <string>

#include "xbase.hpp"
#include "xbase_security.hpp"
#include "xbase_security_policy.hpp"
#include "xbase_security_runtime.hpp"
#include "xbase_security_tests.hpp"

using namespace xbase::security;
using namespace xbase::security::policy;
using namespace xbase::security::runtime;

void cmd_SECURITY(xbase::DbArea& A, std::istringstream& in)
{
    (void)A; // SECURITY does not operate on a work area

    std::string sub;
    if (!(in >> sub))
        sub = "HELP";

    // Normalize to uppercase
    for (auto& c : sub)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    // ------------------------------------------------------------
    // SHOW — display active policy
    // ------------------------------------------------------------
    if (sub == "SHOW")
    {
        config policy = strict_profile();
        context ctx(policy);

        std::cout << policy.describe();
        std::cout << "elevated: " << (ctx.is_elevated ? "yes" : "no") << "\n";
        return;
    }

    // ------------------------------------------------------------
    // SELFTEST — run built-in security tests
    // ------------------------------------------------------------
    if (sub == "SELFTEST")
    {
        int failures = xbase::tests::run_xbase_security_tests();

        if (failures == 0)
            std::cout << "All xBase_64 security tests passed.\n";
        else
            std::cout << failures << " security tests failed.\n";

        return;
    }

    // ------------------------------------------------------------
    // RUNTIME — explain runtime enforcement rules
    // ------------------------------------------------------------
    if (sub == "RUNTIME")
    {
        config policy = strict_profile();

        std::cout
            << "x64Base Runtime Security Enforcement:\n"
            << "  • Path canonicalization (no traversal)\n"
            << "  • Header validation required\n"
            << "  • Atomic writes required for structural updates\n"
            << "  • Elevated writes forbidden\n"
            << "  • Plaintext secrets forbidden\n"
            << "  • Secure temp files required\n"
            << "  • User-scoped directories enforced\n"
            << "  • Policy level: " << to_string(policy.security_level) << "\n";

        return;
    }

    // ------------------------------------------------------------
    // HELP — default
    // ------------------------------------------------------------
    std::cout
        << "SECURITY commands:\n"
        << "  SECURITY SHOW       Display active security policy\n"
        << "  SECURITY SELFTEST   Run built-in security tests\n"
        << "  SECURITY RUNTIME    Describe runtime enforcement rules\n";
}