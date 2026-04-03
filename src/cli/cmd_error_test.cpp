// src/cli/cmd_ERROR_TEST.cpp
// Self-test for the xBase_64 error subsystem.

#include <iostream>
#include <sstream>
#include <iomanip>

#include "xbase.hpp"
#include "xbase_error_codes.hpp"
#include "xbase_error_runtime.hpp"
#include "xbase_error_context.hpp"

using namespace xbase::error;

namespace
{
    bool test_ok()
    {
        code c = ok();
        if (!c.ok()) return false;
        if (c.get_severity() != severity::success) return false;
        if (c.get_facility() != facility::general) return false;
        if (c.get_number() != 0) return false;
        return true;
    }

    bool test_packing()
    {
        code c = make_code(severity::error, facility::dbf64, 0x0042);
        if (c.get_severity() != severity::error) return false;
        if (c.get_facility() != facility::dbf64) return false;
        if (c.get_number() != 0x0042) return false;
        return true;
    }

    bool test_known_codes()
    {
        if (e_invalid_argument().get_facility() != facility::general) return false;
        if (e_dbf_header_invalid().get_facility() != facility::dbf64) return false;
        if (e_fpt_block_invalid().get_facility() != facility::fpt64) return false;
        if (e_security_policy_violation().get_facility() != facility::security) return false;
        if (e_cli_parse_error().get_facility() != facility::cli) return false;
        return true;
    }

    bool test_hresult_bridge()
    {
        code c = e_dbf_header_invalid();
        std::uint32_t hr = to_hresult(c);
        code back = from_hresult(hr);
        return back.value == c.value;
    }

    void print_result(const char* name, bool ok_flag)
    {
        std::cout << "  " << std::left << std::setw(20) << name
                  << (ok_flag ? "OK" : "FAIL") << "\n";
    }
}

void cmd_ERROR_TEST(xbase::DbArea& A, std::istringstream& in)
{
    (void)A;
    (void)in;

    std::cout << "ERROR subsystem self-test:\n";

    bool ok1 = test_ok();
    bool ok2 = test_packing();
    bool ok3 = test_known_codes();
    bool ok4 = test_hresult_bridge();

    print_result("ok()", ok1);
    print_result("packing", ok2);
    print_result("known codes", ok3);
    print_result("HRESULT bridge", ok4);

    bool all_ok = ok1 && ok2 && ok3 && ok4;

    if (all_ok) {
        clear_last_error();
        std::cout << "All tests passed.\n";
    } else {
        // Set a canonical error to indicate failure of the test suite.
        set_last_error(e_unknown());
        std::cout << "One or more tests FAILED.\n";
    }
}