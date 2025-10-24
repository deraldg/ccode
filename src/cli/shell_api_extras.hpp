#include "dli/shell_api_extras.hpp"
#include <string>
#include <vector>
namespace xbase { class DbArea; }

bool dli_api_get_index_fields(xbase::DbArea& /*area*/, std::vector<std::string>& outNames) {
    outNames.clear();
    // TODO: wire to real index metadata; leaving empty is safe (caller shows a note)
    return false;
}

bool dli_api_get_field_value(xbase::DbArea& /*area*/, const std::string& /*name*/, std::string& /*out*/) {
    // TODO: fetch field's current value from active record
    return false;
}
