#include "xbase.hpp"
#include "xbase_error_codes.hpp"
#include "xbase_error_context.hpp"

using namespace xbase::error;

bool dbf64_validate_header(const xbase::DbArea& h)
{
    error_guard guard;
    clear_last_error();

    if (h.recLength() == 0) {
        set_last_error(e_dbf_header_invalid());
        return false;
    }
/*
    if (h.headerlength == 0) {
        set_last_error(e_dbf_header_invalid());
        return false;
    }

    set_last_error(ok());
    return true;
*/
}