#include <iostream>
#include <sstream>
#include <string>
#include "command_registry.hpp"
#include "textio.hpp"
#include "xbase.hpp"

// ZAP: Flag ALL records as deleted (soft delete) after confirmation.
// Note: To physically remove them and reclaim space, run PACK afterwards.
namespace {

void cmd_ZAP(xbase::DbArea& area, std::istringstream& iss)
{
    (void)iss;
    if (!area.isOpen()) {
        std::cout << "No table is open. Use USE <file> first.\n";
        return;
    }

    std::cout << "ZAP will DELETE ALL records in '" << area.name()
              << "'. Type YES to confirm: ";
    std::string reply;
    std::getline(std::cin, reply);
    if (textio::up(textio::trim(reply)) != "YES") {
        std::cout << "ZAP cancelled.\n";
        return;
    }

    // Iterate and flag all records as deleted
    if (!area.top() || !area.readCurrent()) {
        std::cout << "Table is empty.\n";
        return;
    }

    int32_t total = area.recCount();
    int32_t count = 0;
    do {
        // Flag current record deleted
        if (!area.deleteCurrent()) {
            std::cout << "Warning: failed to delete record " << area.recno() << "\n";
        } else {
            ++count;
        }
    } while (area.skip(+1) && area.readCurrent());

    std::cout << "ZAP flagged " << count << " of " << total << " records as deleted.\n";
    std::cout << "Run PACK to permanently remove deleted records and compact the file.\n";
}

} // namespace

static bool s_registered = [](){
    static cli::CommandRegistry reg;
    reg.add("ZAP", &cmd_ZAP);
    return true;
}();
